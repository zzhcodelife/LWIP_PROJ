/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - "commonalization" from prior samples and/or documentation extension
 *******************************************************************************/

#include "transport.h"
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "string.h"
#include "FreeRTOS.h"
#include "semphr.h"

static int32_t mysock;

/* 递归互斥锁: 串行化对 mysock 的所有访问 (open/close/send/recv/select)，
 * 防止 mqtt_recv_thread 在 select 时被 mqtt_send_thread 关闭/重开 socket，
 * 进而触发 sockets.c:565 的 select_waiting==0 断言。 */
static SemaphoreHandle_t mqtt_mtx = NULL;

void transport_init(void)
{
    if (mqtt_mtx == NULL) {
        mqtt_mtx = xSemaphoreCreateRecursiveMutex();
    }
}

void transport_lock(void)
{
    if (mqtt_mtx == NULL) {
        transport_init();
    }
    xSemaphoreTakeRecursive(mqtt_mtx, portMAX_DELAY);
}

void transport_unlock(void)
{
    if (mqtt_mtx != NULL) {
        xSemaphoreGiveRecursive(mqtt_mtx);
    }
}
/************************************************************************
** 函数名称: transport_sendPacketBuffer
** 函数功能: 以 TCP 方式发送数据
** 入口参数: unsigned char* buf：数据缓冲区
** int32_t buflen：数据长度
** 出口参数: <0 发送数据失败
************************************************************************/
int32_t transport_sendPacketBuffer(uint8_t *buf, int32_t buflen)
{
	int32_t rc;
	transport_lock();
	rc = write(mysock, buf, buflen);
	transport_unlock();
	return rc;
}
/************************************************************************
** 函数名称: transport_getdata
** 函数功能: 接收 TCP 数据
** 入口参数: unsigned char* buf：数据缓冲区
** int32_t count：数据长度
** 出口参数: <=0 接收数据失败
************************************************************************/
int32_t transport_getdata(uint8_t *buf, int32_t count)
{
	int32_t rc;
	transport_lock();
	rc = recv(mysock, buf, count, 0);
	transport_unlock();
	return rc;
}

/************************************************************************
** 函数名称: transport_open
** 函数功能: 打开一个接口，并且和服务器 建立连接
** 入口参数: char* servip: 服务器域名
** int32_t port: 端口号
** 出口参数: <0 打开连接失败
************************************************************************/
int32_t transport_open(int8_t *servip, int32_t port)
{
	int32_t *sock = &mysock;
    int32_t ret;
    struct sockaddr_in addr;
    struct timeval timeout;
    int flags;
    fd_set wfds, efds;
    int so_err = 0;
    socklen_t so_err_len = sizeof(so_err);

    transport_lock();

    // 初始化服务器信息
    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = PP_HTONS(port);
    addr.sin_addr.s_addr = inet_addr((const char *)servip);

    // 创建 socket
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        transport_unlock();
        return -1;
    }

    /* lwIP 的阻塞 connect 不参考 SO_SNDTIMEO，必须改成
       "非阻塞 connect + select 等待" 才能真正实现 5 秒超时 */
    flags = fcntl(*sock, F_GETFL, 0);
    fcntl(*sock, F_SETFL, flags | O_NONBLOCK);

    // 连接服务器
    ret = connect(*sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret != 0 && errno != EINPROGRESS)
    {
        vTaskDelay(10);
        close(*sock);
        transport_unlock();
        return -1;
    }

    if (ret != 0)
    {
        /* 5 秒内等待 connect 完成: writable=成功, exceptfd=失败 */
        FD_ZERO(&wfds); FD_SET(*sock, &wfds);
        FD_ZERO(&efds); FD_SET(*sock, &efds);
        timeout.tv_sec  = 5;
        timeout.tv_usec = 0;
        ret = select(*sock + 1, NULL, &wfds, &efds, &timeout);
        if (ret <= 0)
        {
            /* 超时或 select 出错 */
            vTaskDelay(10);
            close(*sock);
            transport_unlock();
            return -1;
        }
        if (getsockopt(*sock, SOL_SOCKET, SO_ERROR, &so_err, &so_err_len) < 0
            || so_err != 0)
        {
            vTaskDelay(10);
            close(*sock);
            transport_unlock();
            return -1;
        }
    }

    /* 恢复阻塞模式，让后续 send/recv 维持原行为 */
    fcntl(*sock, F_SETFL, flags);

    // 连接成功，设置接收超时
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    transport_unlock();
    return *sock;
}
/************************************************************************
** 函数名称: transport_close
** 函数功能: 关闭套接字
** 入口参数: unsigned char* buf：数据缓冲区
** int32_t buflen：数据长度
** 出口参数: <0 发送数据失败
************************************************************************/
int32_t transport_close(void)
{
	int32_t rc;
	transport_lock();
	// rc = close(mysock);
	rc = shutdown(mysock, SHUT_WR);
	rc = recv(mysock, NULL, (size_t)0, 0);
	rc = close(mysock);
	transport_unlock();
	return rc;
}