#ifndef FATFS_UTILS_H

#define FATFS_UTILS_H



#include <stdint.h>

#include "ff.h"



#define FATFS_DRIVE          "0:"

/* 上电挂载后创建的 TFTP/升级目录（f_mkdir，已存在则忽略） */
#define FATFS_DIR_ECUUPDATE  FATFS_DRIVE "/EcuUpdate"

#ifndef FATFS_RUN_MKFS

#define FATFS_RUN_MKFS       0   /* 1：上电 f_mkfs；0：仅挂载（卡上已有 FAT，反复写建议用 0） */

#endif



typedef struct {

    volatile uint8_t  mounted;

    volatile uint8_t  mkfs_done;

    volatile FRESULT  last_fr;

} FatFs_Dbg_t;



extern volatile FatFs_Dbg_t g_fatfs;



/* 上电初始化阶段（main 里每步 +1，调试停住只看这个） */

#define INIT_STAGE_NONE    0u

#define INIT_STAGE_TCPIP   1u

#define INIT_STAGE_TFTP    2u

#define INIT_STAGE_FATFS   3u

#define INIT_STAGE_SOCK_READY  4u   /* socketudp 已 bind:69，可发 TFTP 测试 */

extern volatile uint8_t g_init_stage;



uint8_t fatfs_init(void);

/* 卸载卷 → SD_Recover → 再挂载；读失败恢复用，返回 1=成功 */
uint8_t fatfs_recover(void);

#endif


