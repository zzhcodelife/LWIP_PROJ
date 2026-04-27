#include "iperf_client.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#define IPERF_PORT 5001
#define IPERF_BUFSZ (4 * 1024)
static void iperf_client(void *thread_param)
{
    struct netconn *conn;
    int i;
    int ret;
    uint8_t *send_buf;
    uint64_t sentlen;
    u32_t tick1, tick2;
    ip4_addr_t ipaddr;
    send_buf = (uint8_t *)pvPortMalloc(IPERF_BUFSZ);
    if (!send_buf)
        return;
    for (i = 0; i < IPERF_BUFSZ; i++)
        send_buf[i] = i & 0xff;
    while (1)
    {
        conn = netconn_new(NETCONN_TCP);
        if (conn == NULL)
        {
            //printf("create conn failed!\n");
            vTaskDelay(10);
            continue;
        }
        IP4_ADDR(&ipaddr, 192, 168, 0, 181);
        ret = netconn_connect(conn, &ipaddr, 5001);
        if (ret == -1)
        {
            //printf("Connect failed!\n");
            netconn_close(conn);
            vTaskDelay(10);
            continue;
        }
        //printf("Connect to iperf server successful!\n");
        tick1 = sys_now();
        while (1)
        {
            tick2 = sys_now();
            if (tick2 - tick1 >= configTICK_RATE_HZ * 5)
            {
                float f;
                f = (float)(sentlen * configTICK_RATE_HZ / 125 / (tick2 - tick1));
                f /= 1000.0f;
                //printf("send speed = %.4f Mbps!\n", f);
                tick1 = tick2;
                sentlen = 0;
            }
            ret = netconn_write(conn, send_buf, IPERF_BUFSZ, 0);
            if (ret == ERR_OK)
            {
                sentlen += IPERF_BUFSZ;
            }
        }
        // netconn_close(conn);
        // netconn_delete(conn);
    }
}
void iperf_client_init(void)
{
    sys_thread_new("iperf_client", iperf_client, NULL, 2048, 4);
}
