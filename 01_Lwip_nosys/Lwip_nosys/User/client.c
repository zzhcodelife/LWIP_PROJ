#include "client.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
static void client(void *thread_param)
{
    struct netconn *conn;
    err_t ret;
    ip4_addr_t ipaddr;
    uint8_t send_buf[] = "TCP Client Test\r\n";

    while (1)
    {
        // 创建TCP连接
        conn = netconn_new(NETCONN_TCP);
        if (conn == NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // 服务器IP
        IP4_ADDR(&ipaddr, 192, 168, 0, 181);

        // 连接服务器
        ret = netconn_connect(conn, &ipaddr, 5005);
        if (ret != ERR_OK)
        {
            netconn_delete(conn);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 发送数据
        netconn_write(conn, send_buf, sizeof(send_buf), NETCONN_COPY);

        // 关闭并释放
        netconn_close(conn);
        netconn_delete(conn);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void client_init(void)
{
    sys_thread_new("client", client, NULL, 512, 4);
}