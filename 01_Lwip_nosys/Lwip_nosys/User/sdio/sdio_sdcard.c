/**
 * @file    sdio_sdcard.c
 * @brief   正点原子阿波罗 F429 SD 卡底层（HAL SDIO）
 */

#include "sdio_sdcard.h"
#include "stm32f4xx_ll_sdmmc.h"
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"

SD_HandleTypeDef       SDCARD_Handler;
HAL_SD_CardInfoTypeDef SDCardInfo;

#if (SD_DMA_MODE != 0U)
static DMA_HandleTypeDef s_sdRxDma;
static DMA_HandleTypeDef s_sdTxDma;
#endif

static uint8_t s_sdio_align_buf[512] __attribute__((aligned(4)));

static uint8_t SD_MapHalStatus(HAL_StatusTypeDef st)
{
  if (st == HAL_OK)
  {
    return 0U;
  }
  if (st == HAL_TIMEOUT)
  {
    return 0xFFU;
  }
  return 1U;
}

uint8_t SD_Init(void)
{
  uint8_t err;

  SDCARD_Handler.Instance = SDIO;
  SDCARD_Handler.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
  SDCARD_Handler.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
  SDCARD_Handler.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
  SDCARD_Handler.Init.BusWide             = SDIO_BUS_WIDE_1B;
  SDCARD_Handler.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  SDCARD_Handler.Init.ClockDiv            = SDIO_INIT_CLK_DIV;

  if (HAL_SD_Init(&SDCARD_Handler) != HAL_OK)
  {
    return 1U;
  }

  SDCARD_Handler.Init.ClockDiv = 4U;
  (void)SDIO_Init(SDCARD_Handler.Instance, SDCARD_Handler.Init);

  if (HAL_SD_ConfigWideBusOperation(&SDCARD_Handler, SDIO_BUS_WIDE_4B) != HAL_OK)
  {
    return 2U;
  }

  err = SD_GetCardInfo(&SDCardInfo);
  return err;
}

void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_SDIO_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_PULLUP;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF12_SDIO;
  gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
  HAL_GPIO_Init(GPIOC, &gpio);

  gpio.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOD, &gpio);

#if (SD_DMA_MODE != 0U)
  s_sdRxDma.Instance                 = DMA2_Stream3;
  s_sdRxDma.Init.Channel             = DMA_CHANNEL_4;
  s_sdRxDma.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  s_sdRxDma.Init.PeriphInc           = DMA_PINC_DISABLE;
  s_sdRxDma.Init.MemInc              = DMA_MINC_ENABLE;
  s_sdRxDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  s_sdRxDma.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  s_sdRxDma.Init.Mode                = DMA_PFCTRL;
  s_sdRxDma.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  s_sdRxDma.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  s_sdRxDma.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  s_sdRxDma.Init.MemBurst            = DMA_MBURST_INC4;
  s_sdRxDma.Init.PeriphBurst         = DMA_PBURST_INC4;
  __HAL_LINKDMA(hsd, hdmarx, s_sdRxDma);
  HAL_DMA_DeInit(&s_sdRxDma);
  HAL_DMA_Init(&s_sdRxDma);

  s_sdTxDma.Instance                 = DMA2_Stream6;
  s_sdTxDma.Init.Channel             = DMA_CHANNEL_4;
  s_sdTxDma.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  s_sdTxDma.Init.PeriphInc           = DMA_PINC_DISABLE;
  s_sdTxDma.Init.MemInc              = DMA_MINC_ENABLE;
  s_sdTxDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  s_sdTxDma.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  s_sdTxDma.Init.Mode                = DMA_PFCTRL;
  s_sdTxDma.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  s_sdTxDma.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  s_sdTxDma.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  s_sdTxDma.Init.MemBurst            = DMA_MBURST_INC4;
  s_sdTxDma.Init.PeriphBurst         = DMA_PBURST_INC4;
  __HAL_LINKDMA(hsd, hdmatx, s_sdTxDma);
  HAL_DMA_DeInit(&s_sdTxDma);
  HAL_DMA_Init(&s_sdTxDma);

  HAL_NVIC_SetPriority(SDIO_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(SDIO_IRQn);
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 7, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 7, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
#endif
}

uint8_t SD_GetCardInfo(HAL_SD_CardInfoTypeDef *cardinfo)
{
  if (cardinfo == NULL)
  {
    return 1U;
  }
  if (HAL_SD_GetCardInfo(&SDCARD_Handler, cardinfo) != HAL_OK)
  {
    return 1U;
  }
  return 0U;
}

#if (SD_DMA_MODE != 0U)

static uint8_t SD_WaitTransfer(uint32_t timeout_ms, uint8_t is_write)
{
  uint32_t tick = HAL_GetTick();

  while (1)
  {
    if (is_write != 0U)
    {
      if (SDCARD_Handler.State == HAL_SD_STATE_READY)
      {
        return 0U;
      }
    }
    else
    {
      if (SDCARD_Handler.State == HAL_SD_STATE_READY)
      {
        return 0U;
      }
    }
    if ((HAL_GetTick() - tick) >= timeout_ms)
    {
      return 1U;
    }
    taskYIELD();
  }
}

static uint8_t SD_ReadBlocks_DMA(uint32_t *buf, uint32_t sector, uint32_t cnt)
{
  uint8_t err;

  err = SD_MapHalStatus(HAL_SD_ReadBlocks_DMA(&SDCARD_Handler, (uint8_t *)buf, sector, cnt));
  if (err != 0U)
  {
    return err;
  }
  return SD_WaitTransfer(SD_TIMEOUT, 0U);
}

static uint8_t SD_WriteBlocks_DMA(uint32_t *buf, uint32_t sector, uint32_t cnt)
{
  uint8_t err;

  err = SD_MapHalStatus(HAL_SD_WriteBlocks_DMA(&SDCARD_Handler, (uint8_t *)buf, sector, cnt));
  if (err != 0U)
  {
    return err;
  }
  return SD_WaitTransfer(SD_TIMEOUT, 1U);
}

void SDIO_IRQHandler(void)
{
  HAL_SD_IRQHandler(&SDCARD_Handler);
}

void DMA2_Stream3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(SDCARD_Handler.hdmarx);
}

void DMA2_Stream6_IRQHandler(void)
{
  HAL_DMA_IRQHandler(SDCARD_Handler.hdmatx);
}

#else

static uint8_t SD_ReadBlocks_Poll(uint32_t *buf, uint32_t sector, uint32_t cnt)
{
  return SD_MapHalStatus(HAL_SD_ReadBlocks(&SDCARD_Handler, (uint8_t *)buf, sector, cnt, SD_TIMEOUT));
}

static uint8_t SD_WriteBlocks_Poll(uint32_t *buf, uint32_t sector, uint32_t cnt)
{
  return SD_MapHalStatus(HAL_SD_WriteBlocks(&SDCARD_Handler, (uint8_t *)buf, sector, cnt, SD_TIMEOUT));
}

#endif

uint8_t SD_ReadDisk(uint8_t *buf, uint32_t sector, uint32_t cnt)
{
  uint8_t sta = 0U;
  uint32_t n;

  taskENTER_CRITICAL();
#if (SD_DMA_MODE != 0U)
  if (((uint32_t)buf & 3U) != 0U)
  {
    for (n = 0U; n < cnt; n++)
    {
      sta = SD_ReadBlocks_DMA((uint32_t *)s_sdio_align_buf, sector + n, 1U);
      if (sta != 0U)
      {
        break;
      }
      memcpy(buf + (n * 512U), s_sdio_align_buf, 512U);
    }
  }
  else
  {
    sta = SD_ReadBlocks_DMA((uint32_t *)buf, sector, cnt);
  }
#else
  if (((uint32_t)buf & 3U) != 0U)
  {
    for (n = 0U; n < cnt; n++)
    {
      sta = SD_ReadBlocks_Poll((uint32_t *)s_sdio_align_buf, sector + n, 1U);
      if (sta != 0U)
      {
        break;
      }
      memcpy(buf + (n * 512U), s_sdio_align_buf, 512U);
    }
  }
  else
  {
    sta = SD_ReadBlocks_Poll((uint32_t *)buf, sector, cnt);
  }
#endif
  taskEXIT_CRITICAL();
  return sta;
}

uint8_t SD_WriteDisk(uint8_t *buf, uint32_t sector, uint32_t cnt)
{
  uint8_t sta = 0U;
  uint32_t n;

  taskENTER_CRITICAL();
#if (SD_DMA_MODE != 0U)
  if (((uint32_t)buf & 3U) != 0U)
  {
    for (n = 0U; n < cnt; n++)
    {
      memcpy(s_sdio_align_buf, buf + (n * 512U), 512U);
      sta = SD_WriteBlocks_DMA((uint32_t *)s_sdio_align_buf, sector + n, 1U);
      if (sta != 0U)
      {
        break;
      }
    }
  }
  else
  {
    sta = SD_WriteBlocks_DMA((uint32_t *)buf, sector, cnt);
  }
#else
  if (((uint32_t)buf & 3U) != 0U)
  {
    for (n = 0U; n < cnt; n++)
    {
      memcpy(s_sdio_align_buf, buf + (n * 512U), 512U);
      sta = SD_WriteBlocks_Poll((uint32_t *)s_sdio_align_buf, sector + n, 1U);
      if (sta != 0U)
      {
        break;
      }
    }
  }
  else
  {
    sta = SD_WriteBlocks_Poll((uint32_t *)buf, sector, cnt);
  }
#endif
  taskEXIT_CRITICAL();
  return sta;
}
