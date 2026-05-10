#ifndef NET_DIAG_H
#define NET_DIAG_H

/* ============================================================
 * 临时网络诊断变量
 * 目的: 排查 STM32+lwIP 经 PC ICS 连 OneNET 握手失败 (SYN/ACK 丢失?)
 *
 * 用法:
 *   1. 在调试器 Watch 窗口加入下面所有 g_diag_* 变量.
 *   2. 让 mqtt_recv_thread 至少跑一次完整的 connect 重试周期.
 *   3. 按 "判读速查表" (见 net_diag.c 顶部注释) 逐层定位.
 *
 * OneNET 服务器 IP = 218.201.45.7
 *   host order   = 0xDAC92D07
 *   network ord. = 0x072DC9DA  (little-endian CPU 上 .addr 的值)
 *
 * tcp_state enum (lwIP):
 *   0=CLOSED  1=LISTEN   2=SYN_SENT  3=SYN_RCVD   4=ESTABLISHED
 *   5=FIN_WAIT_1  6=FIN_WAIT_2  7=CLOSE_WAIT
 *   8=CLOSING 9=LAST_ACK 10=TIME_WAIT
 *
 * 排查完毕直接整体删掉这套埋点即可, 不影响业务.
 * ============================================================ */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === ETH RX 层 (low_level_input) === */
extern volatile uint32_t g_diag_eth_rx_count;       /* 成功取到的 ETH 帧数 */
extern volatile uint32_t g_diag_eth_rx_get_fail;    /* HAL_ETH_GetReceivedFrame 失败次数 (DMA 描述符错误/ES) */
extern volatile uint16_t g_diag_eth_rx_last_len;
extern volatile uint16_t g_diag_eth_rx_last_etype;  /* network order: 0x0008=IPv4, 0x0608=ARP (字节翻转) */
extern volatile uint8_t  g_diag_eth_rx_last_smac[6];
extern volatile uint8_t  g_diag_eth_rx_over10;      /* 收满 10 帧后置 1, 不再清零 (供调试器条件断点/Watch 触发) */

/* === IP4 层 (ip4_input) === */
extern volatile uint32_t g_diag_ip_rx_count;
extern volatile uint32_t g_diag_ip_rx_chkdrop;      /* CHECKSUM_CHECK_IP=0 时永为 0 (硬件丢的看不到) */
extern volatile uint32_t g_diag_ip_rx_lendrop;
extern volatile uint32_t g_diag_ip_rx_last_src;     /* host order */
extern volatile uint32_t g_diag_ip_rx_last_dst;
extern volatile uint8_t  g_diag_ip_rx_last_proto;   /* 1=ICMP 6=TCP 17=UDP */
extern volatile uint32_t g_diag_ip_from_onenet;     /* src == 218.201.45.7 的包数 */

/* === TCP 层 (tcp_input) === */
extern volatile uint32_t g_diag_tcp_rx_count;
extern volatile uint32_t g_diag_tcp_rx_chkdrop;     /* CHECKSUM_CHECK_TCP=0 时永为 0 */
extern volatile uint32_t g_diag_tcp_rx_last_src_ip; /* host order */
extern volatile uint16_t g_diag_tcp_rx_last_src_port;
extern volatile uint16_t g_diag_tcp_rx_last_dst_port;
extern volatile uint8_t  g_diag_tcp_rx_last_flags;  /* MQTT 关心: 0x12=SYN+ACK, 0x10=ACK, 0x04=RST, 0x18=PSH+ACK */
extern volatile uint32_t g_diag_tcp_rx_last_seq;
extern volatile uint32_t g_diag_tcp_rx_last_ack;
extern volatile uint32_t g_diag_tcp_from_onenet;
extern volatile uint8_t  g_diag_tcp_from_onenet_last_flags;

/* === TCP PCB 状态扫描 (tcp_active_pcbs 链表) === */
#define DIAG_PCB_MAX 6
typedef struct {
    uint32_t local_ip;       /* host order */
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint8_t  state;          /* tcp_state, 见上方注释 */
    uint8_t  used;
    uint16_t flags;
    uint32_t snd_nxt;
    uint32_t rcv_nxt;
} diag_pcb_t;

extern volatile diag_pcb_t g_diag_pcbs[DIAG_PCB_MAX];
extern volatile uint32_t   g_diag_pcb_scan_count;

/* 在锁保护下扫描 lwIP tcp_active_pcbs 链表, 复制到 g_diag_pcbs */
void diag_dump_pcbs(void);

#ifdef __cplusplus
}
#endif

#endif /* NET_DIAG_H */
