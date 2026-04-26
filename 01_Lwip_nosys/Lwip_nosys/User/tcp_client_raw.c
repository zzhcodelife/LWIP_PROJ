#include "tcp_client_raw.h"
#include "lwip/netif.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/init.h"
#include "netif/etharp.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include <stdio.h>
#include <string.h>
static struct tcp_pcb *client_pcb = NULL;

// 包装函数：适配 sys_timeout 类型
static void tcp_client_reconnect(void *arg)
{
    TCP_Client_Raw_Init();
}

static void client_err(void *arg, err_t err)
{
    // 错误回调中，pcb 已被内核释放，只需清空指针
    client_pcb = NULL;
    // 延时1秒后重连，避免端口复用
    sys_timeout(1000, tcp_client_reconnect, NULL);
}

static err_t client_send(void *arg, struct tcp_pcb *tpcb)
{
    uint8_t send_buf[] = "This is a TCP Client test...\n";
    // 发送数据到服务器
    tcp_write(tpcb, send_buf, sizeof(send_buf), 1);
    return ERR_OK;
}
static err_t client_recv(void *arg,
                         struct tcp_pcb *tpcb,
                         struct pbuf *p,
                         err_t err)
{
    if (p != NULL)
    {
        tcp_recved(tpcb, p->tot_len);
        /* 返回接收到的数据 */
        tcp_write(tpcb, p->payload, p->tot_len, 1);
        memset(p->payload, 0, p->tot_len);
        pbuf_free(p);
        return ERR_OK;
    }

    // 服务器断开，正常关闭连接
    tcp_close(tpcb);
    client_pcb = NULL;
    sys_timeout(1000, tcp_client_reconnect, NULL);
    return ERR_OK;
}

static err_t client_connected(void *arg,
                              struct tcp_pcb *pcb,
                              err_t err)
{
    //printf("connected ok!\n");
    // 注册一个周期性回调函数
    tcp_poll(pcb, client_send, 2);
    // 注册一个接收函数
    tcp_recv(pcb, client_recv);
    return ERR_OK;
}

void TCP_Client_Raw_Init(void)
{
   ip4_addr_t server_ip;

    // 防止重复创建 pcb
    if (client_pcb != NULL)
        return;

    // 创建新的 TCP 控制块
    client_pcb = tcp_new();
    if (client_pcb == NULL)
    {
        sys_timeout(1000, tcp_client_reconnect, NULL);
        return;
    }

    // 先注册错误回调，再调用 connect
    tcp_err(client_pcb, client_err);
    


    IP4_ADDR(&server_ip, 192, 168, 0, 181);

    // 发起连接，并判断返回值
    err_t ret = tcp_connect(client_pcb, &server_ip, 5012, client_connected);
    if (ret != ERR_OK)
    {
        // 连接失败，用 abort 释放 pcb
        tcp_abort(client_pcb);
        client_pcb = NULL;
        sys_timeout(1000, tcp_client_reconnect, NULL);
    }
}