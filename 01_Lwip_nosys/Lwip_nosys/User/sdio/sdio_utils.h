#ifndef __SDIO_UTILS_H
#define __SDIO_UTILS_H

#include <stdint.h>

/* Watch g_sdio_utils 调试（无串口/LCD） */
typedef struct
{
  volatile uint32_t run_count;
  volatile uint8_t  phase;           /* 见 SDIO_UTILS_PHASE_* */
  volatile uint8_t  init_ok;         /* 1=SD_Init 成功 */
  volatile uint8_t  init_err;        /* SD_Init 返回值：0/1/2 */
  volatile uint8_t  info_ok;
  volatile uint32_t card_type;
  volatile uint8_t  manufacturer_id;
  volatile uint16_t rca;
  volatile uint32_t capacity_mb;
  volatile uint32_t block_size;
  volatile uint8_t  last_read_ok;
  volatile uint8_t  last_write_ok;
  volatile uint32_t last_secaddr;
  volatile uint32_t last_seccnt;
  volatile uint32_t last_io_err;
  volatile uint32_t sample_u32_0;    /* 读回缓冲区前 4 字节 */
  volatile uint32_t sample_u32_1;
  volatile uint32_t write_pattern;   /* 写测试 pattern 校验用 */
  volatile uint8_t  compare_ok;
  volatile uint8_t  done;
} SDIO_UtilsDbg_t;

#define SDIO_UTILS_PHASE_IDLE       0U
#define SDIO_UTILS_PHASE_INIT       1U
#define SDIO_UTILS_PHASE_INFO       2U
#define SDIO_UTILS_PHASE_WRITE      3U
#define SDIO_UTILS_PHASE_READ       4U
#define SDIO_UTILS_PHASE_COMPARE    5U
#define SDIO_UTILS_PHASE_DONE       9U

extern volatile SDIO_UtilsDbg_t g_sdio_utils;

void SDIO_Utils_ShowCardInfo(void);
uint8_t SDIO_Utils_TestRead(uint32_t secaddr, uint32_t seccnt);
uint8_t SDIO_Utils_TestWrite(uint32_t secaddr, uint32_t seccnt);
void SDIO_Utils_RunOnce(void);

/* 与正点原子例程同名接口（内部转 g_sdio_utils） */
#define show_sdcard_info   SDIO_Utils_ShowCardInfo
#define sd_test_read(sec, cnt)   SDIO_Utils_TestRead((sec), (cnt))
#define sd_test_write(sec, cnt)  SDIO_Utils_TestWrite((sec), (cnt))

#endif
