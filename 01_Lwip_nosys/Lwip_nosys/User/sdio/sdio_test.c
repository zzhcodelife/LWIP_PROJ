/**
  * @file    sdio_test.c
  * @brief   SD 卡读写测试（无串口时用 g_sd_test 调试）
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sdio/bsp_sdio_sd.h"
#include "sdio/sdio_test.h"

volatile SD_TestFlags_t g_sd_test;

#define BLOCK_START_ADDR         4096U
#define NUM_OF_BLOCKS            1U
#define BUFFER_WORDS_SIZE        ((BLOCKSIZE * NUM_OF_BLOCKS) >> 2)
#define SD_CARD_WAIT_TIMEOUT_MS  30000U

static uint32_t aTxBuffer[BUFFER_WORDS_SIZE] __attribute__((aligned(4)));
static uint32_t aRxBuffer[BUFFER_WORDS_SIZE] __attribute__((aligned(4)));

static void Fill_Buffer(uint32_t *pBuffer, uint32_t uwBufferLenght, uint32_t uwOffset);
static uint8_t Buffercmp(uint32_t* pBuffer1, uint32_t* pBuffer2, uint16_t BufferLength);
static void SD_Test_ResetFlags(void);
static uint8_t SD_WaitCardReady(uint32_t timeout_ms, uint8_t wait_stage);
static void SD_Test_MarkHalError(void);
static uint8_t SD_Test_InitCard(void);
static uint8_t SD_Test_DoWriteRead(void);

static void SD_Test_ResetFlags(void)
{
  g_sd_test.init       = SD_TEST_NA;
  g_sd_test.erase      = SD_TEST_NA;
  g_sd_test.write      = SD_TEST_NA;
  g_sd_test.read       = SD_TEST_NA;
  g_sd_test.compare    = SD_TEST_NA;
  g_sd_test.all_pass   = 0U;
  g_sd_test.done       = 0U;
  g_sd_test.stage      = SD_TEST_STAGE_NONE;
  g_sd_test.wait_fail  = 0U;
  g_sd_test.hal_err    = 0U;
  g_sd_test.log_blocks = 0U;
  g_sd_test.sd_state   = 0U;
  g_sd_test.read_probe = SD_TEST_NA;
  g_sd_test.init_fail  = 0U;
  g_sd_test.fail_stage = 0U;
}

static void SD_Test_MarkHalError(void)
{
  g_sd_test.hal_err = uSdHandle.ErrorCode;
}

static uint8_t SD_WaitCardReady(uint32_t timeout_ms, uint8_t wait_stage)
{
  TickType_t start = xTaskGetTickCount();
  TickType_t limit = pdMS_TO_TICKS(timeout_ms);

  g_sd_test.stage = wait_stage;

  while (BSP_SD_GetCardState() != SD_TRANSFER_OK)
  {
    if ((xTaskGetTickCount() - start) >= limit)
    {
      g_sd_test.wait_fail = wait_stage;
      SD_Test_MarkHalError();
      return 0U;
    }
    taskYIELD();
  }
  return 1U;
}

static uint8_t SD_Test_InitCard(void)
{
  if (BSP_SD_Init() != MSD_OK)
  {
    g_sd_test.init_fail = 1U;
    SD_Test_MarkHalError();
    return 0U;
  }

  g_sd_test.log_blocks = uSdHandle.SdCard.LogBlockNbr;

  if ((g_sd_test.log_blocks != 0U) &&
      (BLOCK_START_ADDR + NUM_OF_BLOCKS > g_sd_test.log_blocks))
  {
    g_sd_test.init_fail = 2U;
    g_sd_test.hal_err = HAL_SD_ERROR_ADDR_OUT_OF_RANGE;
    return 0U;
  }

  return 1U;
}

static uint8_t SD_Test_DoWriteRead(void)
{
  uint8_t SD_state;

  if (BSP_SD_PrepForTransfer() != MSD_OK)
  {
    g_sd_test.fail_stage = 3U;
    g_sd_test.sd_state = (uint8_t)uSdHandle.State;
    SD_Test_MarkHalError();
    return 0U;
  }

  g_sd_test.sd_state = (uint8_t)uSdHandle.State;

#if (SDIO_TEST_READ_PROBE != 0U)
  g_sd_test.stage = SD_TEST_STAGE_READ;
  SD_state = BSP_SD_ReadBlocks(aRxBuffer, BLOCK_START_ADDR, 1U,
                               SD_CARD_WAIT_TIMEOUT_MS);
  if (SD_state != MSD_OK)
  {
    g_sd_test.fail_stage = 4U;
    g_sd_test.read_probe = SD_TEST_FAIL;
    SD_Test_MarkHalError();
    return 0U;
  }
  g_sd_test.read_probe = SD_TEST_OK;
  (void)BSP_SD_PrepForTransfer();
#else
  g_sd_test.read_probe = SD_TEST_NA;
#endif

  Fill_Buffer(aTxBuffer, BUFFER_WORDS_SIZE, 0x22FF);

#if (SDIO_TEST_USE_POLLING != 0U)
  g_sd_test.stage = SD_TEST_STAGE_WRITE;
  g_sd_test.sd_state = (uint8_t)uSdHandle.State;
  SD_state = BSP_SD_WriteBlocks(aTxBuffer, BLOCK_START_ADDR, NUM_OF_BLOCKS,
                                SD_CARD_WAIT_TIMEOUT_MS);
  if (SD_state != MSD_OK)
  {
    g_sd_test.fail_stage = 5U;
    g_sd_test.write = SD_TEST_FAIL;
    SD_Test_MarkHalError();
    return 0U;
  }
  g_sd_test.write = SD_TEST_OK;

  (void)BSP_SD_PrepForTransfer();

  g_sd_test.stage = SD_TEST_STAGE_READ;
  SD_state = BSP_SD_ReadBlocks(aRxBuffer, BLOCK_START_ADDR, NUM_OF_BLOCKS,
                               SD_CARD_WAIT_TIMEOUT_MS);
  if (SD_state != MSD_OK)
  {
    g_sd_test.fail_stage = 6U;
    g_sd_test.read = SD_TEST_FAIL;
    SD_Test_MarkHalError();
    return 0U;
  }
  g_sd_test.read = SD_TEST_OK;
#else
  g_sd_test.stage = SD_TEST_STAGE_WRITE;
  SD_state = BSP_SD_WriteBlocks_DMA(aTxBuffer, BLOCK_START_ADDR, NUM_OF_BLOCKS);
  if (!SD_WaitCardReady(SD_CARD_WAIT_TIMEOUT_MS, SD_TEST_STAGE_WRITE_WAIT))
  {
    g_sd_test.write = SD_TEST_FAIL;
    return 0U;
  }
  if (SD_state != MSD_OK)
  {
    g_sd_test.write = SD_TEST_FAIL;
    SD_Test_MarkHalError();
    return 0U;
  }
  g_sd_test.write = SD_TEST_OK;

  g_sd_test.stage = SD_TEST_STAGE_READ;
  SD_state = BSP_SD_ReadBlocks_DMA(aRxBuffer, BLOCK_START_ADDR, NUM_OF_BLOCKS);
  if (!SD_WaitCardReady(SD_CARD_WAIT_TIMEOUT_MS, SD_TEST_STAGE_READ_WAIT))
  {
    g_sd_test.read = SD_TEST_FAIL;
    return 0U;
  }
  if (SD_state != MSD_OK)
  {
    g_sd_test.read = SD_TEST_FAIL;
    SD_Test_MarkHalError();
    return 0U;
  }
  g_sd_test.read = SD_TEST_OK;
#endif

  g_sd_test.stage = SD_TEST_STAGE_COMPARE;
  if (Buffercmp(aTxBuffer, aRxBuffer, BUFFER_WORDS_SIZE) > 0)
  {
    g_sd_test.fail_stage = 7U;
    g_sd_test.compare = SD_TEST_FAIL;
    return 0U;
  }
  g_sd_test.compare = SD_TEST_OK;
  return 1U;
}

void SD_Test(void)
{
  uint8_t SD_state = MSD_OK;
  uint8_t proceed = 0U;

  SD_Test_ResetFlags();
  g_sd_test.run_count++;

  g_sd_test.stage = SD_TEST_STAGE_INIT;

#if (SDIO_TEST_INIT_EACH_RUN != 0U)
  if (SD_Test_InitCard() != 0U)
  {
    g_sd_test.init = SD_TEST_OK;
    proceed = 1U;
  }
  else
  {
    g_sd_test.init = SD_TEST_FAIL;
  }
#else
  {
    static uint8_t s_sd_inited = 0U;

    if ((s_sd_inited == 0U) && (SD_Test_InitCard() != 0U))
    {
      s_sd_inited = 1U;
    }
    if (s_sd_inited != 0U)
    {
      g_sd_test.init = SD_TEST_OK;
      proceed = 1U;
    }
    else
    {
      g_sd_test.init = SD_TEST_FAIL;
    }
  }
#endif

  (void)SD_state;

#if (SDIO_TEST_SKIP_ERASE == 0U)
  if (proceed != 0U)
  {
    g_sd_test.stage = SD_TEST_STAGE_ERASE;
    SD_state = BSP_SD_Erase(BLOCK_START_ADDR,
                            BLOCK_START_ADDR + NUM_OF_BLOCKS - 1U);

    if (!SD_WaitCardReady(SD_CARD_WAIT_TIMEOUT_MS, SD_TEST_STAGE_ERASE_WAIT))
    {
      g_sd_test.erase = SD_TEST_FAIL;
      proceed = 0U;
    }
    else if (SD_state != MSD_OK)
    {
      g_sd_test.erase = SD_TEST_FAIL;
      SD_Test_MarkHalError();
      proceed = 0U;
    }
    else
    {
      g_sd_test.erase = SD_TEST_OK;
    }
  }
#else
  g_sd_test.erase = SD_TEST_NA;
#endif

  if (proceed != 0U)
  {
    (void)SD_Test_DoWriteRead();
  }

#if (SDIO_TEST_SKIP_ERASE != 0U)
  if ((g_sd_test.init == SD_TEST_OK) &&
      (g_sd_test.write == SD_TEST_OK) &&
      (g_sd_test.read == SD_TEST_OK) &&
      (g_sd_test.compare == SD_TEST_OK))
#else
  if ((g_sd_test.init == SD_TEST_OK) &&
      (g_sd_test.erase == SD_TEST_OK) &&
      (g_sd_test.write == SD_TEST_OK) &&
      (g_sd_test.read == SD_TEST_OK) &&
      (g_sd_test.compare == SD_TEST_OK))
#endif
  {
    g_sd_test.all_pass = 1U;
  }

  g_sd_test.stage = SD_TEST_STAGE_FINISH;
  g_sd_test.done = 1U;
}

static void Fill_Buffer(uint32_t *pBuffer, uint32_t uwBufferLenght, uint32_t uwOffset)
{
  uint32_t tmpIndex = 0;

  for (tmpIndex = 0; tmpIndex < uwBufferLenght; tmpIndex++)
  {
    pBuffer[tmpIndex] = tmpIndex + uwOffset;
  }
}

static uint8_t Buffercmp(uint32_t* pBuffer1, uint32_t* pBuffer2, uint16_t BufferLength)
{
  while (BufferLength--)
  {
    if (*pBuffer1 != *pBuffer2)
    {
      return 1;
    }
    pBuffer1++;
    pBuffer2++;
  }
  return 0;
}
