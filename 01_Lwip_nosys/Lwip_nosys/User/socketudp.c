/**
 * @file socketudp.c
 * @brief TFTP 服务器网络线程（Socket API，UDP 69）+ FatFs 工作线程（tftp_fs）。
 *
 * PC 写（PUT）:
 *   tftp -i 192.168.195.120 PUT C:\Users\m1878\Desktop\ecu_upgrade.txt EcuUpdate/ecu_upgrade.txt
 */
#include "socketudp.h"

#include <string.h>
#include <stdio.h>

#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "tftp_fs.h"
#include "fatfs_utils.h"
#include "FreeRTOS.h"
#include "task.h"

#define TFTP_PORT              69
#define TFTP_BLOCK_SIZE        512U
#define TFTP_HDR_SIZE          4U
#define TFTP_MAX_PKT           (TFTP_HDR_SIZE + TFTP_BLOCK_SIZE)

/* RFC1350 前 4 字节：opcode + block/error（线上大端）；强转后须 lwip_ntohs/lwip_htons */
typedef struct __attribute__((packed)) {
    uint16_t opcode;
    uint16_t block;
} tftp_hdr_t;
#define TFTP_SOCK_TIMEOUT_SEC  5
#define TFTP_NET_STACK_WORDS   1536U
#define TFTP_NET_TASK_PRIO     5U
#define TFTP_SOCK_ERR_OK       1U
#define TFTP_SOCK_ERR_TASK     2U
#define TFTP_SOCK_ERR_SOCKET   3U
#define TFTP_SOCK_ERR_BIND     4U

#define TFTP_RRQ   1U
#define TFTP_WRQ   2U
#define TFTP_DATA  3U
#define TFTP_ACK   4U
#define TFTP_ERROR 5U

#define TFTP_ERR_FILE_NOT_FOUND  1
#define TFTP_ERR_ACCESS_VIOLATION 2
#define TFTP_ERR_DISK_FULL       3
#define TFTP_ERR_ILLEGAL_OP      4

typedef enum {
    SESS_IDLE = 0,
    SESS_READ,
    SESS_WRITE,
} sess_state_t;

typedef struct {
    sess_state_t          state;
    struct sockaddr_in    peer;
    socklen_t             peer_len;
    uint16_t              block;
    uint16_t              last_len;
    uint8_t               last_pkt[TFTP_MAX_PKT];
    uint16_t              last_pkt_len;
} tftp_session_t;

volatile Tftp_Dbg_t g_tftp_dbg;

extern struct netif gnetif;

static tftp_session_t s_sess;
static int            s_listen_sock = -1;

static size_t bounded_strlen(const char *s, size_t max)
{
    size_t n = 0U;

    while (n < max && s[n] != '\0') {
        n++;
    }
    return n;
}

static int str_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = (char)((*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a);
        char cb = (char)((*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b);
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == *b);
}

static void sess_reset(void)
{
    tftp_fs_close();
    memset(&s_sess, 0, sizeof(s_sess));
    g_tftp_dbg.active    = 0U;
    g_tftp_dbg.sess_idle = 0U;
}

static int sock_send(int sock, const void *buf, int len)
{
    int n;

    g_tftp_dbg.sock_send_enter++;

    if (sock < 0) {
        g_tftp_dbg.tx_fail++;
        return -1;
    }
    n = sendto(sock, buf, len, 0,
               (struct sockaddr *)&s_sess.peer, s_sess.peer_len);
    g_tftp_dbg.last_send_n = n;
    g_tftp_dbg.mark        = 4U;   /* 断点：看 last_send_n、tx_fail */
    if (n > 0) {
        g_tftp_dbg.tx_packets++;
    } else {
        g_tftp_dbg.tx_fail++;
    }
    return n;
}

static void send_error(int sock, uint16_t code, const char *msg)
{
    uint8_t pkt[TFTP_HDR_SIZE + 64];
    tftp_hdr_t *hdr = (tftp_hdr_t *)(void *)pkt;
    size_t msg_len = strlen(msg) + 1U;

    if (msg != NULL && strcmp(msg, "read failed") == 0) {
        g_tftp_dbg.err_step = 6U;   /* 断点：必定进（不被 fs_step 覆盖） */
        g_tftp_dbg.get_step = 6U;
    }

    hdr->opcode = lwip_htons(TFTP_ERROR);
    hdr->block  = lwip_htons(code);
    memcpy(pkt + TFTP_HDR_SIZE, msg, msg_len);
    (void)sock_send(sock, pkt, (int)(TFTP_HDR_SIZE + msg_len));
    sess_reset();
}

static void send_ack(int sock, uint16_t block)
{
    uint8_t pkt[sizeof(tftp_hdr_t)];
    tftp_hdr_t *hdr = (tftp_hdr_t *)(void *)pkt;

    hdr->opcode = lwip_htons(TFTP_ACK);
    hdr->block  = lwip_htons(block);
    (void)sock_send(sock, pkt, (int)sizeof(pkt));
}

static int send_data_block(int sock, uint16_t block, const uint8_t *data, uint16_t len)
{
    uint8_t pkt[TFTP_MAX_PKT];
    tftp_hdr_t *hdr = (tftp_hdr_t *)(void *)pkt;

    hdr->opcode = lwip_htons(TFTP_DATA);
    hdr->block  = lwip_htons(block);
    memcpy(pkt + TFTP_HDR_SIZE, data, len);

    s_sess.last_pkt_len = (uint16_t)(TFTP_HDR_SIZE + len);
    memcpy(s_sess.last_pkt, pkt, s_sess.last_pkt_len);
    s_sess.last_len = len;

    return sock_send(sock, pkt, (int)s_sess.last_pkt_len);
}

static void resend_last(int sock)
{
    if (s_sess.last_pkt_len > 0U) {
        (void)sock_send(sock, s_sess.last_pkt, (int)s_sess.last_pkt_len);
    }
}

static int is_rrq_or_wrq(const uint8_t *buf, int len)
{
    uint16_t op;

    if (len < 2) {
        return 0;
    }
    op = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    return (op == TFTP_RRQ || op == TFTP_WRQ) ? 1 : 0;
}

static int parse_request(const uint8_t *buf, int len, char *fname, size_t fname_sz,
                       char *mode, size_t mode_sz, uint16_t *opcode)
{
    const char *p;
    const char *mode_p;
    size_t fn_len;
    size_t mode_len;

    if (len < 4) {
        return 0;
    }

    /* RRQ/WRQ 仅 2 字节 opcode，随后即文件名；不能用 4 字节 tftp_hdr_t（会吃掉文件名前 2 字符） */
    *opcode = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    if (*opcode != TFTP_RRQ && *opcode != TFTP_WRQ) {
        return 0;
    }

    p = (const char *)buf + 2;
    fn_len = bounded_strlen(p, (size_t)len - 2);
    if (fn_len == 0U || fn_len >= fname_sz || p[fn_len] != '\0') {
        return 0;
    }
    memcpy(fname, p, fn_len + 1U);

    mode_p = p + fn_len + 1U;
    if (mode_p >= (const char *)buf + len) {
        return 0;
    }
    mode_len = bounded_strlen(mode_p, (size_t)((const char *)buf + len - mode_p));
    if (mode_len == 0U || mode_len >= mode_sz || mode_p[mode_len] != '\0') {
        return 0;
    }
    memcpy(mode, mode_p, mode_len + 1U);

    return 1;
}

static int begin_read(int sock, const char *fname)
{
    FRESULT fr;
    uint8_t data[TFTP_BLOCK_SIZE];
    uint16_t nread;

    g_tftp_dbg.phase = 2U;

    if (g_fatfs.mounted == 0U) {
        send_error(sock, TFTP_ERR_ACCESS_VIOLATION, "volume not mounted");
        return 0;
    }

    fr = tftp_fs_open_read(fname);
    if (fr != FR_OK) {
        send_error(sock, TFTP_ERR_FILE_NOT_FOUND, "open read failed");
        return 0;
    }

    g_tftp_dbg.phase = 3U;
    g_tftp_dbg.mark    = 3U;   /* 断点：f_open 成功 */

    s_sess.state = SESS_READ;
    s_sess.block = 1U;
    g_tftp_dbg.active    = 1U;
    g_tftp_dbg.sess_idle = 1U;
    g_tftp_dbg.is_write = 0U;
    g_tftp_dbg.block = s_sess.block;

    g_tftp_dbg.get_step = 4U;
    fr = tftp_fs_read_block(data, TFTP_BLOCK_SIZE, &nread);
    if (fr != FR_OK) {
        g_tftp_dbg.last_fs_fr = (uint8_t)fr;
        send_error(sock, TFTP_ERR_DISK_FULL, "read failed");
        return 0;
    }

    g_tftp_dbg.get_step = 5U;
    if (send_data_block(sock, s_sess.block, data, nread) < 0) {
        g_tftp_dbg.get_step = 8U;
        sess_reset();
        return 0;
    }

    g_tftp_dbg.get_step = 7U;
    if (nread < TFTP_BLOCK_SIZE) {
        sess_reset();
    }
    return 1;
}

static int begin_write(int sock, const char *fname)
{
    FRESULT fr;

    if (g_fatfs.mounted == 0U) {
        send_error(sock, TFTP_ERR_ACCESS_VIOLATION, "volume not mounted");
        return 0;
    }

    tftp_fs_close();
    fr = tftp_fs_open_write(fname);
    if (fr != FR_OK) {
        tftp_fs_close();
        vTaskDelay(pdMS_TO_TICKS(100));
        fr = tftp_fs_open_write(fname);
    }
    if (fr != FR_OK) {
        g_tftp_dbg.last_fs_fr = (uint8_t)fr;
        send_error(sock, TFTP_ERR_ACCESS_VIOLATION, "open write failed");
        return 0;
    }

    s_sess.state = SESS_WRITE;
    s_sess.block = 1U;
    g_tftp_dbg.active    = 1U;
    g_tftp_dbg.sess_idle = 2U;
    g_tftp_dbg.is_write  = 1U;
    g_tftp_dbg.block = 0U;

    send_ack(sock, 0U);
    return 1;
}

static void handle_ack(int sock, uint16_t block)
{
    uint8_t data[TFTP_BLOCK_SIZE];
    uint16_t nread;
    FRESULT fr;

    g_tftp_dbg.last_ack = block;

    if (s_sess.state != SESS_READ) {
        return;
    }

    if (block == (uint16_t)(s_sess.block - 1U)) {
        g_tftp_dbg.get_step = 1U;   /* 断点：PC 重传 ACK，只 resend 上一 DATA */
        resend_last(sock);
        return;
    }

    if (block != s_sess.block) {
        g_tftp_dbg.get_step = 2U;   /* 断点：块号不对，静默不发 */
        return;
    }

    if (s_sess.last_len < TFTP_BLOCK_SIZE) {
        sess_reset();
        return;
    }

    g_tftp_dbg.get_step = 3U;       /* 断点：ACK 匹配，即将 block++ 并读卡 */
    s_sess.block++;
    g_tftp_dbg.block = s_sess.block;

    g_tftp_dbg.get_step = 4U;       /* 断点：卡在 read 时=get_step 4 或 40 */
    fr = tftp_fs_read_block(data, TFTP_BLOCK_SIZE, &nread);
    if (fr != FR_OK) {
        g_tftp_dbg.last_fs_fr = (uint8_t)fr;
        send_error(sock, TFTP_ERR_DISK_FULL, "read failed");  /* err_step=6 在 send_error 内 */
        return;
    }

    g_tftp_dbg.get_step = 5U;       /* 断点：读 OK，即将 send DATA */
    if (send_data_block(sock, s_sess.block, data, nread) < 0) {
        g_tftp_dbg.get_step = 8U;
        sess_reset();
        return;
    }

    g_tftp_dbg.get_step = 7U;       /* 断点：本块 DATA 已发出 */
    if (nread < TFTP_BLOCK_SIZE) {
        sess_reset();
    }
}

static void handle_data(int sock, uint16_t block, const uint8_t *data, uint16_t len)
{
    FRESULT fr;

    if (s_sess.state != SESS_WRITE) {
        return;
    }

    if (block == (uint16_t)(s_sess.block - 1U)) {
        send_ack(sock, block);
        return;
    }

    if (block != s_sess.block) {
        return;
    }

    fr = tftp_fs_write_block(data, len);
    if (fr != FR_OK) {
        send_error(sock, TFTP_ERR_DISK_FULL, "write failed");
        return;
    }

    send_ack(sock, block);
    g_tftp_dbg.block = block;

    if (len < TFTP_BLOCK_SIZE) {
        sess_reset();
        return;
    }

    s_sess.block++;
}

static void handle_request(int sock, const uint8_t *buf, int len,
                           const struct sockaddr_in *from, socklen_t from_len)
{
    char fname[96];
    char mode[16];
    uint16_t opcode;

    if (s_sess.state != SESS_IDLE) {
        sess_reset();
    }

    if (!parse_request(buf, len, fname, sizeof(fname), mode, sizeof(mode), &opcode)) {
        send_error(sock, TFTP_ERR_ILLEGAL_OP, "bad request");
        return;
    }

    if (!str_ieq(mode, "octet") && !str_ieq(mode, "binary")) {
        send_error(sock, TFTP_ERR_ILLEGAL_OP, "mode not octet");
        return;
    }

    memcpy(&s_sess.peer, from, sizeof(*from));
    s_sess.peer_len = from_len;
    g_tftp_dbg.phase = 1U;

    /* 全程复用 listen_sock(69) + sendto(peer) */

    g_tftp_dbg.mark = 2U;   /* 断点：已收 RRQ/WRQ */

    if (opcode == TFTP_RRQ) {
        (void)begin_read(sock, fname);
    } else {
        (void)begin_write(sock, fname);
    }
}

static void handle_datagram(int sock, const uint8_t *buf, int len)
{
    const tftp_hdr_t *hdr = (const tftp_hdr_t *)(const void *)buf;
    uint16_t opcode;
    uint16_t block;

    if (len < (int)sizeof(tftp_hdr_t)) {
        return;
    }

    g_tftp_dbg.rx_packets++;
    opcode = lwip_ntohs(hdr->opcode);
    block  = lwip_ntohs(hdr->block);
    g_tftp_dbg.last_opcode = opcode;
    g_tftp_dbg.sess_state  = (uint8_t)s_sess.state;

    if (s_sess.state == SESS_IDLE) {
        if (opcode == TFTP_RRQ || opcode == TFTP_WRQ) {
            handle_request(sock, buf, len, &s_sess.peer, s_sess.peer_len);
        }
        return;
    }

    /* 同类型重复 RRQ/WRQ（Windows 常连发）：忽略，勿 reset 打断正在传的块 */
    if (opcode == TFTP_RRQ || opcode == TFTP_WRQ) {
        if ((opcode == TFTP_RRQ && s_sess.state == SESS_READ) ||
            (opcode == TFTP_WRQ && s_sess.state == SESS_WRITE)) {
            return;
        }
        /* GET↔PUT 切换或异常残留：结束旧会话再开新请求 */
        sess_reset();
        handle_request(sock, buf, len, &s_sess.peer, s_sess.peer_len);
        return;
    }

    switch (opcode) {
    case TFTP_ACK:
        handle_ack(sock, block);
        break;
    case TFTP_DATA:
        handle_data(sock, block, buf + TFTP_HDR_SIZE, (uint16_t)(len - (int)TFTP_HDR_SIZE));
        break;
    default:
        send_error(sock, TFTP_ERR_ILLEGAL_OP, "unexpected opcode");
        break;
    }
}

static void socketudp_thread(void *arg)
{
    uint8_t buf[TFTP_MAX_PKT + 4];
    struct sockaddr_in addr;
    struct sockaddr_in from;
    socklen_t from_len;
    struct timeval tv;
    int recv_len;
    int rsock;

    (void)arg;

    g_tftp_dbg.sock_ready = 0U;
    g_tftp_dbg.sock_err   = 0U;

    while (!netif_is_link_up(&gnetif)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    s_listen_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_listen_sock < 0) {
        g_tftp_dbg.sock_err = TFTP_SOCK_ERR_SOCKET;
        vTaskDelete(NULL);
        return;
    }

    {
        int reuse = 1;
        (void)setsockopt(s_listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(TFTP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    {
        int i;

        for (i = 0; i < 20; i++) {
            if (bind(s_listen_sock, (struct sockaddr *)&addr, sizeof(addr)) >= 0) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (i >= 20) {
            g_tftp_dbg.sock_err = TFTP_SOCK_ERR_BIND;
            closesocket(s_listen_sock);
            s_listen_sock = -1;
            vTaskDelete(NULL);
            return;
        }
    }

    g_tftp_dbg.sock_ready = 1U;
    g_tftp_dbg.sock_err   = TFTP_SOCK_ERR_OK;
    g_tftp_dbg.mark       = 1U;   /* 断点：上电就绪，看 sock_ready/mounted/fs_worker_ok */
    g_init_stage          = INIT_STAGE_SOCK_READY;

    tv.tv_sec  = TFTP_SOCK_TIMEOUT_SEC;
    tv.tv_usec = 0;
    (void)setsockopt(s_listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (;;) {
        rsock = s_listen_sock;
        if (rsock < 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        (void)setsockopt(rsock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        from_len = sizeof(from);
        recv_len = recvfrom(rsock, buf, sizeof(buf), 0,
                            (struct sockaddr *)&from, &from_len);
        if (recv_len < 0) {
            if (s_sess.state == SESS_READ && s_sess.last_pkt_len > 0U) {
                resend_last(s_listen_sock);
            } else if (s_sess.state != SESS_IDLE) {
                /* PC 中断/传完未收尾：必须关文件，否则反复 PUT 会 open write failed */
                sess_reset();
            }
            continue;
        }

        g_tftp_dbg.recv_ok++;   /* 断点：UDP 已到应用层；>0 才可能有 handle_request */

        if (s_sess.state == SESS_IDLE || is_rrq_or_wrq(buf, recv_len)) {
            memcpy(&s_sess.peer, &from, sizeof(from));
            s_sess.peer_len = from_len;
        }

        handle_datagram(rsock, buf, recv_len);
    }
}

void socketudp_init(void)
{
    BaseType_t ret;

    tftp_fs_worker_start();
    ret = xTaskCreate(socketudp_thread, "socketudp_thread", TFTP_NET_STACK_WORDS,
                      NULL, TFTP_NET_TASK_PRIO, NULL);
    if (ret != pdPASS) {
        g_tftp_dbg.sock_ready = 0U;
        g_tftp_dbg.sock_err   = TFTP_SOCK_ERR_TASK;
    }
}
