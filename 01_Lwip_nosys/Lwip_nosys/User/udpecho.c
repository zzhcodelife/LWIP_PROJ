#include "udpecho.h"
#include "lwip/opt.h"
#if LWIP_NETCONN
#include "lwip/api.h"
#include "lwip/sys.h"
/*--------------------------------------------------------------------*/
static void udpecho_thread(void *arg)
{
struct netconn *conn;
struct netbuf *buf;
char buffer[4096];
err_t err;
LWIP_UNUSED_ARG(arg);
#if LWIP_IPV6
conn = netconn_new(NETCONN_UDP_IPV6);
netconn_bind(conn, IP6_ADDR_ANY, 5001);
#else /* LWIP_IPV6 */
conn = netconn_new(NETCONN_UDP);
netconn_bind(conn, IP_ADDR_ANY, 5001);
#endif /* LWIP_IPV6 */
LWIP_ERROR("udpecho: invalid conn", (conn != NULL), return;);
while (1)
{
err = netconn_recv(conn, &buf);
if (err == ERR_OK)
{
if (netbuf_copy(buf, buffer, sizeof(buffer))!= buf->p->tot_len)
{
LWIP_DEBUGF(LWIP_DBG_ON, ("netbuf_copy failed\n"));
}
else
{
buffer[buf->p->tot_len] = '\0';
err = netconn_send(conn, buf);
if (err != ERR_OK)
{
LWIP_DEBUGF(LWIP_DBG_ON, ("netconn_send failed: %d\n",(int)err));
}
else
{
LWIP_DEBUGF(LWIP_DBG_ON, ("got %s\n", buffer));
}
}
netbuf_delete(buf);
}
}
}
/*--------------------------------------------------------------------*/
void
udpecho_init(void)
{
sys_thread_new("udpecho_thread", udpecho_thread, NULL, 2048, 4);
} 
#endif /* LWIP_NETCONN */

