#ifndef FATFS_UTILS_H
#define FATFS_UTILS_H

#include <stdint.h>
#include "ff.h"

#define FATFS_TEST_DRIVE     "0:"
#define FATFS_TEST_DIR       FATFS_TEST_DRIVE "/fat_test"
#define FATFS_TEST_FILE      FATFS_TEST_DIR "/hello.txt"

#ifndef FATFS_TEST_AUTO_MKFS
#define FATFS_TEST_AUTO_MKFS  0
#endif

#ifndef FATFS_TEST_RUN_MKFS
#define FATFS_TEST_RUN_MKFS   1
#endif

/* Keil Watch: g_fatfs_test（断点建议：fatfs_test_run 末尾，或 dbg_fail 内） */
typedef struct
{
    volatile uint8_t  running;
    volatile uint8_t  pass;
    volatile uint8_t  mounted;
    volatile uint8_t  mkfs_done;

    volatile FRESULT  last_fr;
    volatile uint8_t  step;           /* 失败步骤号；成功结束为 99 */

    volatile uint32_t free_clusters;
    volatile uint32_t total_clusters;

    volatile uint32_t bytes_written;
    volatile uint32_t bytes_read;
    volatile uint32_t bytes_expected;
    volatile uint8_t  data_match;

    volatile uint32_t stat_file_size;
    volatile uint32_t dir_entries;
    volatile char     last_dir_name[32];  /* readdir 最后一条文件名 */

    volatile uint32_t sd_sector_count;    /* GET_SECTOR_COUNT / LogBlockNbr */
    volatile uint32_t disk_last_sector;   /* 最后一次失败的扇区号 */
    volatile uint8_t  disk_last_sta;      /* 最后一次 SD_Read/WriteDisk 返回值 */
    volatile uint32_t disk_last_hal_err;  /* 失败时 SDCARD_Handler.ErrorCode */
} FatFs_TestDbg_t;

extern volatile FatFs_TestDbg_t g_fatfs_test;

uint8_t fatfs_test_run(void);

#endif
