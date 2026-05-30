#ifndef _SDIO_UTILS_H
#define _SDIO_UTILS_H

#include <stdint.h>

/* Keil Watch 添加 g_sdio_utils 查看原 printf 内容 */
typedef struct
{
    volatile uint8_t  card_type_tag;   /* 1=SDSC V1.1  2=SDSC V2.0  3=SDHC  4=MMC */
    volatile uint8_t  manufacturer_id;
    volatile uint16_t rca;
    volatile uint32_t capacity_mb;
    volatile uint32_t block_size;

    volatile uint32_t read_sector;
    volatile uint8_t  read_sta;
    volatile uint8_t  read_ok;
    volatile uint32_t read_byte_sum;

    volatile uint32_t test_sector;
    volatile uint8_t  write_sta;
    volatile uint8_t  write_ok;
    volatile uint8_t  malloc_fail;

    volatile uint8_t  compare_ok;
    volatile uint32_t compare_fail_index;
    volatile uint8_t  compare_fail_expect;
    volatile uint8_t  compare_fail_actual;

    volatile uint8_t  verify_ok_4096;   /* sd_test_rw_verify(4096,1) 返回值 */
    volatile uint8_t  verify_ok_0;      /* sd_test_rw_verify(0,1) 返回值 */
} SDIO_Dbg_t;

extern volatile SDIO_Dbg_t g_sdio_utils;

void show_sdcard_info(void);
void sd_test_read(uint32_t secaddr, uint32_t seccnt);
void sd_test_write(uint32_t secaddr, uint32_t seccnt);
uint8_t sd_test_rw_verify(uint32_t secaddr, uint32_t seccnt);
void sd_test_rw_verify_dual(void);

#endif
