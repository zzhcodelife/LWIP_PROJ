#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "http_netconn_server.h"
#if LWIP_NETCONN
#ifndef HTTPD_DEBUG
#define HTTPD_DEBUG LWIP_DBG_OFF
#endif
static const char http_html_hdr[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-type: text/html; charset=utf-8\r\n" // 关键：声明UTF-8编码
    "\r\n";

static const char http_index_html[] =
    "<html><head>"
        "<title>Congrats!</title>"
        "<meta charset=\"UTF-8\">" // 关键：HTML内也声明编码，双保险
    "</head>"
    "<body>"
        "<h1 align=\"center\">Hello World!</h1>"
        "<h2 align=\"center\">Welcome to Fire lwIP HTTP Server!</h2>"
        "<p align=\"center\">This is a small test page, served by httpserver-netconn.</p>"
        "<p align=\"center\"><a href=\"http://www.firebbs.cn/forum.php/\">"
        "<font size=\"6\">野火电子论坛</font></a></p>"
        "<p align=\"center\"><img src=\"https://picsum.photos/id/119/600/300\" width=\"300\" alt=\"星空银河\"/></p>"
    "</body></html>";
/** Serve one HTTP connection accepted in the http thread */
static void
http_server_netconn_serve(struct netconn *conn)
{   
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;
    /* 读取数据 */
    err = netconn_recv(conn, &inbuf);
    if (err == ERR_OK)
    {
        netbuf_data(inbuf, (void **)&buf, &buflen);
        /* 判断是不是 HTTP 的 GET 命令 */
        if (buflen >= 5 &&
            buf[0] == 'G' &&
            buf[1] == 'E' &&
            buf[2] == 'T' &&
            buf[3] == ' ' &&
            buf[4] == '/')
        {
            /* 发送数据头 */
            netconn_write(conn, http_html_hdr,
                          sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
            /* 发送网页数据 */
            netconn_write(conn, http_index_html,
                          sizeof(http_index_html) - 1, NETCONN_NOCOPY);
        }
    }
    netconn_close(conn); /* 关闭连接 */
    /* 释放 inbuf */
    netbuf_delete(inbuf);
}
/** The main function, never returns! */
static void
http_server_netconn_thread(void *arg)
{
    struct netconn *conn, *newconn;
    err_t err;
    LWIP_UNUSED_ARG(arg);
    /* 创建 netconn 连接结构 */
    /* 绑定端口号与 IP 地址，端口号默认是 80 */
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, IP_ADDR_ANY, 80);
    LWIP_ERROR("http_server: invalid conn", (conn != NULL), return;);
    /* 监听 */
    netconn_listen(conn);
    do
    {
        // 处理连接请求
        err = netconn_accept(conn, &newconn);
        if (err == ERR_OK)
        {
            // 发送网页数据
            http_server_netconn_serve(newconn);
            // 删除连接结构
            netconn_delete(newconn);
        }
    } while (err == ERR_OK);
    // 关闭
    netconn_close(conn);
    netconn_delete(conn);
}
void http_server_netconn_init(void)
{
    sys_thread_new("http_server_netconn", http_server_netconn_thread, NULL, 2048, 4);
}
#endif
