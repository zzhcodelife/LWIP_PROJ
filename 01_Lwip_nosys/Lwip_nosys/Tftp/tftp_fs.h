/**
 * @file tftp_fs.h
 * @brief TFTP：FatFs 工作线程（队列 RPC）+ 调试状态（网络侧在 socketudp）。
 */
#ifndef TFTP_FS_H
#define TFTP_FS_H

#include <stdint.h>
#include "ff.h"

#define TFTP_FS_PATH_MAX  96U

/* sock_err: 0=未就绪 1=OK 2=建任务失败 3=socket失败 4=bind失败 */
typedef struct {
    volatile uint8_t  sock_ready;
    volatile uint8_t  sock_err;
    volatile uint8_t  active;
    volatile uint8_t  is_write;
    volatile uint16_t block;
    volatile uint32_t rx_packets;
    volatile uint32_t tx_packets;
    volatile uint32_t tx_fail;
    volatile int32_t  last_send_n;     /* 最近一次 sendto 返回值 */
    volatile uint8_t  fs_worker_ok;    /* 1: tftp_fs 任务已创建 */
    volatile uint32_t sock_send_enter; /* 进入 sock_send 次数 */
    volatile uint8_t  phase;           /* 0无 1收RRQ 2begin_read 3已open */
    volatile uint8_t  mark;            /* 1=69就绪 2=RRQ 3=open读OK 4=sendto(会覆盖get_step观感) */
    volatile uint8_t  get_step;        /* socketudp GET 路径（勿与 fs_step 混看） */
    volatile uint8_t  fs_step;         /* tftp_fs 任务：40=f_read中 41=f_read返回 */
    volatile uint8_t  err_step;        /* 最近一次 GET 错误，sess_reset 不清除 */
    volatile uint16_t last_ack;        /* 最近一次 handle_ack 的块号 */
    /* get_step: 0无 1重发DATA 2ACK块号丢弃 3ACK匹配 4等read_block 5读OK 7已发DATA 8sendto失败
     * err_step: 6=read failed 44=RPC超时 45=f_read非OK 46=已fatfs_recover */
    volatile uint32_t recv_ok;         /* recvfrom 成功次数(>0) */
    volatile uint16_t last_opcode;     /* 上一包 TFTP opcode */
    volatile uint8_t  sess_state;      /* 0=IDLE 1=READ 2=WRITE */
    volatile uint8_t  last_fs_fr;      /* 最近一次 f_open/f_read 等 FRESULT */
    volatile char     last_fs_path[64];/* 对应 Fat 路径，断点停住后看 */
    volatile uint8_t  sess_idle;       /* 0=无会话 1=READ 2=WRITE，超时复位后变 0 */
} Tftp_Dbg_t;

extern volatile Tftp_Dbg_t g_tftp_dbg;

void tftp_fs_worker_start(void);

FRESULT tftp_fs_open_read(const char *tftp_name);
FRESULT tftp_fs_open_write(const char *tftp_name);
FRESULT tftp_fs_read_block(uint8_t *buf, uint16_t buf_len, uint16_t *out_len);
FRESULT tftp_fs_write_block(const uint8_t *buf, uint16_t len);
void tftp_fs_close(void);

#endif /* TFTP_FS_H */
