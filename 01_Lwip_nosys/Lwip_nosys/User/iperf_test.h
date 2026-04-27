#ifndef LWIP_IPERF_H
#define LWIP_IPERF_H
#define TCP_SERVER_THREAD_NAME "iperf_server"
#define TCP_SERVER_THREAD_STACKSIZE 1024
#define TCP_SERVER_THREAD_PRIO 4
void iperf_server(void *thread_param);
void iperf_server_init(void);
#endif