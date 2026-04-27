#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define SYS_LIGHTWEIGHT_PROT    1
#define NO_SYS                  0
#define NO_SYS_NO_TIMERS        0

/* ---------- Memory options ---------- */
#define MEM_ALIGNMENT           4
#define MEM_SIZE                (12*1024)

#define MEMP_NUM_PBUF           32
#define MEMP_NUM_UDP_PCB        2
#define MEMP_NUM_TCP_PCB        4
#define MEMP_NUM_TCP_PCB_LISTEN 2
#define MEMP_NUM_TCP_SEG        24
#define MEMP_NUM_SYS_TIMEOUT    8

/* ---------- Pbuf options ---------- */
#define PBUF_POOL_SIZE          24
#define PBUF_POOL_BUFSIZE       1524

/* ---------- TCP options ---------- */
#define LWIP_TCP                1
#define TCP_TTL                 255

#define TCP_QUEUE_OOSEQ         0
#define TCP_MSS                 (1500 - 40)
#define TCP_SND_BUF             (8*TCP_MSS)
#define TCP_SND_QUEUELEN        (2 * TCP_SND_BUF / TCP_MSS) // 自动计算，不报错
#define TCP_WND                 (8*TCP_MSS)

/* ---------- ICMP options ---------- */
#define LWIP_ICMP               1

/* ---------- DHCP options ---------- */
#define LWIP_DHCP               0

/* ---------- UDP options ---------- */
#define LWIP_UDP                0
#define UDP_TTL                 255

/* ---------- Statistics options ---------- */
#define LWIP_STATS              0
#define LWIP_PROVIDE_ERRNO      1

#define LWIP_NETIF_LINK_CALLBACK 0

/* ---------- Checksum options ---------- */
#define CHECKSUM_BY_HARDWARE

#ifdef CHECKSUM_BY_HARDWARE
#define CHECKSUM_GEN_IP         0
#define CHECKSUM_GEN_UDP        0
#define CHECKSUM_GEN_TCP        0
#define CHECKSUM_CHECK_IP       0
#define CHECKSUM_CHECK_UDP      0
#define CHECKSUM_CHECK_TCP      0
#define CHECKSUM_GEN_ICMP       0
#endif

/* ---------- Netconn / Socket ---------- */
#define LWIP_NETCONN            1
#define LWIP_SOCKET             1

/* ---------- OS & Thread options ---------- */
#define DEFAULT_UDP_RECVMBOX_SIZE       10
#define DEFAULT_TCP_RECVMBOX_SIZE       10
#define DEFAULT_ACCEPTMBOX_SIZE         10
#define DEFAULT_THREAD_STACKSIZE        1024

#define TCPIP_THREAD_NAME              "lwip"
#define TCPIP_THREAD_STACKSIZE          3072
#define TCPIP_MBOX_SIZE                 12
#define TCPIP_THREAD_PRIO               4

/* ---------- Debug ---------- */
//#define LWIP_DEBUG 1

#endif /* __LWIPOPTS_H */