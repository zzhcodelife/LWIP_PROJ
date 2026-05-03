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

static int32_t mysock;
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
	rc = write(mysock, buf, buflen);
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
	rc = recv(mysock, buf, count, 0);
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
    
    // 初始化服务器信息
    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = PP_HTONS(port);
    addr.sin_addr.s_addr = inet_addr((const char *)servip);
    
    // 创建 socket
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        return -1;
    }
    
    // 关键修改：设置发送超时，避免 connect 阻塞太久
    struct timeval timeout;
    timeout.tv_sec = 5;  // 5秒连接超时
    timeout.tv_usec = 0;
    setsockopt(*sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // 连接服务器
    ret = connect(*sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret != 0)
    {
        // 关键：先获取错误码
        //int32_t err = errno;
        
        // 关键：延迟一会儿再关闭，让 LwIP 完成清理
        vTaskDelay(10);  // FreeRTOS 延迟 10ms
        
        // 关闭链接
        close(*sock);
        
        //printf("connect failed, errno=%d\n", err);
        return -1;
    }
    
    // 连接成功，恢复默认超时（或设置接收超时）
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
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
	// rc = close(mysock);
	rc = shutdown(mysock, SHUT_WR);
	rc = recv(mysock, NULL, (size_t)0, 0);
	rc = close(mysock);
	return rc;
}