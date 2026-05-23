#ifndef __SDIO_TEST_H
#define __SDIO_TEST_H

#include <stdint.h>

/* 1=跳过擦除 */
#define SDIO_TEST_SKIP_ERASE    1U
/* 1=轮询读写（HAL 内部等完成，不依赖 DMA 中断）；0=DMA+Wait */
#define SDIO_TEST_USE_POLLING    1U
/* 0=仅首次 Init；1=每轮 DeInit（易触发 log_blocks=0，一般保持 0） */
#define SDIO_TEST_INIT_EACH_RUN  0U
/* 1=写之前先读 1 块；0=直接写（读超时常误判为 write 失败） */
#define SDIO_TEST_READ_PROBE     0U

/* 0=未执行  1=成功  2=失败（Watch 查看 g_sd_test） */
#define SD_TEST_NA      0U
#define SD_TEST_OK      1U
#define SD_TEST_FAIL    2U

/* stage：当前执行到哪一步（done=0 时看这里判断是否卡在 while） */
#define SD_TEST_STAGE_NONE        0U
#define SD_TEST_STAGE_INIT        1U
#define SD_TEST_STAGE_ERASE       2U
#define SD_TEST_STAGE_ERASE_WAIT  3U
#define SD_TEST_STAGE_WRITE       4U
#define SD_TEST_STAGE_WRITE_WAIT  5U
#define SD_TEST_STAGE_READ        6U
#define SD_TEST_STAGE_READ_WAIT   7U
#define SD_TEST_STAGE_COMPARE     8U
#define SD_TEST_STAGE_FINISH      9U

typedef struct
{
  volatile uint32_t run_count;
  volatile uint8_t  init;
  volatile uint8_t  erase;
  volatile uint8_t  write;
  volatile uint8_t  read;
  volatile uint8_t  compare;
  volatile uint8_t  all_pass;
  volatile uint8_t  done;
  volatile uint8_t  stage;
  volatile uint8_t  wait_fail;   /* DMA 模式下 Wait 超时阶段 */
  volatile uint32_t hal_err;     /* uSdHandle.ErrorCode；0x4 命令超时 0x8 数据CRC 0x40 超时 */
  volatile uint32_t log_blocks;  /* 卡总块数 */
  volatile uint8_t  sd_state;  /* uSdHandle.State，写前应为 1(READY) */
  volatile uint8_t  read_probe;  /* READ_PROBE：1=OK 2=FAIL 0=未测 */
  volatile uint8_t  init_fail;   /* 0=无 1=BSP_SD_Init 2=块地址校验 */
  volatile uint8_t  fail_stage;  /* 3=prep 4=read_probe 5=write 6=read 7=compare */
} SD_TestFlags_t;

extern volatile SD_TestFlags_t g_sd_test;

void SD_Test(void);

#endif
