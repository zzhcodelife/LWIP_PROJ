#include "client.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
static void client(void *thread_param)
{
    struct netconn *conn;
    int ret;
    ip4_addr_t ipaddr;
    uint8_t send_buf[]= "This is a TCP Client test...\n";
    while (1)
    {
        conn = netconn_new(NETCONN_TCP); 
        if (conn == NULL)
        {
            //printf("create conn failed!\n");
            vTaskDelay(10);
            continue;
        }
        IP4_ADDR(&ipaddr,192,168,0,181); 
        ret = netconn_connect(conn,&ipaddr,5001); 
        if (ret == -1)
        {
           // printf("Connect failed!\n");
            netconn_close(conn); 
            vTaskDelay(10);
            continue;
        }
        //printf("Connect to iperf server successful!\n");
        while (1)
        {
            ret = netconn_write(conn,send_buf,sizeof(send_buf),0); 
            vTaskDelay(1000);
        }
    }
}
void
client_init(void)
{
    sys_thread_new("client", client, NULL, 512, 4);
}