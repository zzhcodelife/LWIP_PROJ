/**
 * @file    sdio_utils.c
 * @brief   正点原子 SD 实验逻辑（无 printf/LCD/按键，用 g_sdio_utils 调试）
 */

#include "sdio_utils.h"
#include "sdio_sdcard.h"
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"

volatile SDIO_UtilsDbg_t g_sdio_utils;

/* 远离 FAT 引导区的测试扇区 */
#define SDIO_UTILS_TEST_SECTOR   4096U
#define SDIO_UTILS_TEST_COUNT    1U

static uint8_t s_io_buf[512] __attribute__((aligned(4)));

void SDIO_Utils_ShowCardInfo(void)
{
  HAL_SD_CardCIDTypeDef cid;

  g_sdio_utils.phase = SDIO_UTILS_PHASE_INFO;

  if (SD_GetCardInfo(&SDCardInfo) != 0U)
  {
    g_sdio_utils.info_ok = 0U;
    return;
  }

  g_sdio_utils.info_ok        = 1U;
  g_sdio_utils.card_type      = SDCardInfo.CardType;
  g_sdio_utils.rca            = (uint16_t)SDCardInfo.RelCardAdd;
  g_sdio_utils.block_size     = SDCardInfo.LogBlockSize;
  g_sdio_utils.capacity_mb    = (SDCardInfo.LogBlockNbr * SDCardInfo.LogBlockSize) >> 20;

  if (HAL_SD_GetCardCID(&SDCARD_Handler, &cid) == HAL_OK)
  {
    g_sdio_utils.manufacturer_id = cid.ManufacturerID;
  }
}

uint8_t SDIO_Utils_TestRead(uint32_t secaddr, uint32_t seccnt)
{
  uint8_t sta;

  g_sdio_utils.phase         = SDIO_UTILS_PHASE_READ;
  g_sdio_utils.last_secaddr  = secaddr;
  g_sdio_utils.last_seccnt   = seccnt;

  sta = SD_ReadDisk(s_io_buf, secaddr, seccnt);
  g_sdio_utils.last_io_err   = sta;
  g_sdio_utils.last_read_ok  = (sta == 0U) ? 1U : 0U;

  if (sta == 0U)
  {
    g_sdio_utils.sample_u32_0 = *(uint32_t *)&s_io_buf[0];
    g_sdio_utils.sample_u32_1 = *(uint32_t *)&s_io_buf[4];
  }

  return sta;
}

uint8_t SDIO_Utils_TestWrite(uint32_t secaddr, uint32_t seccnt)
{
  uint32_t i;
  uint32_t bytes = seccnt * 512U;
  uint8_t sta;

  g_sdio_utils.phase         = SDIO_UTILS_PHASE_WRITE;
  g_sdio_utils.last_secaddr  = secaddr;
  g_sdio_utils.last_seccnt   = seccnt;

  for (i = 0U; i < bytes; i++)
  {
    s_io_buf[i] = (uint8_t)(i * 3U);
  }
  g_sdio_utils.write_pattern = *(uint32_t *)&s_io_buf[0];

  sta = SD_WriteDisk(s_io_buf, secaddr, seccnt);
  g_sdio_utils.last_io_err    = sta;
  g_sdio_utils.last_write_ok  = (sta == 0U) ? 1U : 0U;

  return sta;
}

void SDIO_Utils_RunOnce(void)
{
  uint8_t init_ret;

  g_sdio_utils.done = 0U;
  g_sdio_utils.compare_ok = 0U;

  g_sdio_utils.phase = SDIO_UTILS_PHASE_INIT;
  while (1)
  {
    init_ret = SD_Init();
    g_sdio_utils.init_err = init_ret;
    if (init_ret == 0U)
    {
      break;
    }
    g_sdio_utils.init_ok = 0U;
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  g_sdio_utils.init_ok = 1U;
  g_sdio_utils.run_count++;
  SDIO_Utils_ShowCardInfo();

  if (SDIO_Utils_TestWrite(SDIO_UTILS_TEST_SECTOR, SDIO_UTILS_TEST_COUNT) != 0U)
  {
    g_sdio_utils.phase = SDIO_UTILS_PHASE_DONE;
    g_sdio_utils.done  = 1U;
    return;
  }

  if (SDIO_Utils_TestRead(SDIO_UTILS_TEST_SECTOR, SDIO_UTILS_TEST_COUNT) != 0U)
  {
    g_sdio_utils.phase = SDIO_UTILS_PHASE_DONE;
    g_sdio_utils.done  = 1U;
    return;
  }

  g_sdio_utils.phase = SDIO_UTILS_PHASE_COMPARE;
  {
    uint32_t i;
    uint8_t ok = 1U;

    for (i = 0U; i < (SDIO_UTILS_TEST_COUNT * 512U); i++)
    {
      if (s_io_buf[i] != (uint8_t)(i * 3U))
      {
        ok = 0U;
        break;
      }
    }
    g_sdio_utils.compare_ok = ok;
  }

  g_sdio_utils.phase = SDIO_UTILS_PHASE_DONE;
  g_sdio_utils.done  = 1U;
}
