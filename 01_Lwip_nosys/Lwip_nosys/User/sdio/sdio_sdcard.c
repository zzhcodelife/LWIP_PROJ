#include "sdio_sdcard.h"
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_ll_sdmmc.h"

SD_HandleTypeDef       SDCARD_Handler;
HAL_SD_CardInfoTypeDef SDCardInfo;
DMA_HandleTypeDef      SDTxDMAHandler, SDRxDMAHandler;

__align(4) uint8_t SDIO_DATA_BUFFER[512];

uint8_t SD_Init(void)
{
    uint8_t SD_Error;

    SDCARD_Handler.Instance = SDIO;
    SDCARD_Handler.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
    SDCARD_Handler.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    SDCARD_Handler.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
    SDCARD_Handler.Init.BusWide = SDIO_BUS_WIDE_1B;
    SDCARD_Handler.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    SDCARD_Handler.Init.ClockDiv = SDIO_TRANSFER_CLK_DIV;

    SD_Error = (uint8_t)HAL_SD_Init(&SDCARD_Handler);
    if (SD_Error != SD_OK)
    {
        return 1;
    }

    SD_Error = (uint8_t)HAL_SD_GetCardInfo(&SDCARD_Handler, &SDCardInfo);
    if (SD_Error != SD_OK)
    {
        return 1;
    }

    SD_Error = (uint8_t)HAL_SD_ConfigWideBusOperation(&SDCARD_Handler, SDIO_BUS_WIDE_4B);
    if (SD_Error != SD_OK)
    {
        return 2;
    }
    return 0;
}

void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_SDIO_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_Initure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_Initure.Mode = GPIO_MODE_AF_PP;
    GPIO_Initure.Pull = GPIO_PULLUP;
    GPIO_Initure.Speed = GPIO_SPEED_HIGH;
    GPIO_Initure.Alternate = GPIO_AF12_SDIO;
    HAL_GPIO_Init(GPIOC, &GPIO_Initure);

    GPIO_Initure.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &GPIO_Initure);

#if (SD_DMA_MODE == 1)
    HAL_NVIC_SetPriority(SDIO_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(SDIO_IRQn);

    SDRxDMAHandler.Instance = DMA2_Stream3;
    SDRxDMAHandler.Init.Channel = DMA_CHANNEL_4;
    SDRxDMAHandler.Init.Direction = DMA_PERIPH_TO_MEMORY;
    SDRxDMAHandler.Init.PeriphInc = DMA_PINC_DISABLE;
    SDRxDMAHandler.Init.MemInc = DMA_MINC_ENABLE;
    SDRxDMAHandler.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    SDRxDMAHandler.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    SDRxDMAHandler.Init.Mode = DMA_PFCTRL;
    SDRxDMAHandler.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    SDRxDMAHandler.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    SDRxDMAHandler.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    SDRxDMAHandler.Init.MemBurst = DMA_MBURST_INC4;
    SDRxDMAHandler.Init.PeriphBurst = DMA_PBURST_INC4;

    __HAL_LINKDMA(hsd, hdmarx, SDRxDMAHandler);
    HAL_DMA_DeInit(&SDRxDMAHandler);
    HAL_DMA_Init(&SDRxDMAHandler);

    SDTxDMAHandler.Instance = DMA2_Stream6;
    SDTxDMAHandler.Init.Channel = DMA_CHANNEL_4;
    SDTxDMAHandler.Init.Direction = DMA_MEMORY_TO_PERIPH;
    SDTxDMAHandler.Init.PeriphInc = DMA_PINC_DISABLE;
    SDTxDMAHandler.Init.MemInc = DMA_MINC_ENABLE;
    SDTxDMAHandler.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    SDTxDMAHandler.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    SDTxDMAHandler.Init.Mode = DMA_PFCTRL;
    SDTxDMAHandler.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    SDTxDMAHandler.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    SDTxDMAHandler.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    SDTxDMAHandler.Init.MemBurst = DMA_MBURST_INC4;
    SDTxDMAHandler.Init.PeriphBurst = DMA_PBURST_INC4;

    __HAL_LINKDMA(hsd, hdmatx, SDTxDMAHandler);
    HAL_DMA_DeInit(&SDTxDMAHandler);
    HAL_DMA_Init(&SDTxDMAHandler);

    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
#endif
}

uint8_t SD_GetCardInfo(HAL_SD_CardInfoTypeDef *cardinfo)
{
    return (uint8_t)HAL_SD_GetCardInfo(&SDCARD_Handler, cardinfo);
}

#if (SD_DMA_MODE == 1)

static uint8_t SD_WaitTransferDone(SD_HandleTypeDef *hsd)
{
    uint32_t tickstart = HAL_GetTick();

    while (HAL_SD_GetCardState(hsd) != HAL_SD_CARD_TRANSFER)
    {
        if ((HAL_GetTick() - tickstart) >= SD_TIMEOUT)
        {
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return 0;
}

uint8_t SD_ReadBlocks_DMA(uint32_t *buf, uint64_t sector, uint32_t blocksize, uint32_t cnt)
{
    uint8_t err = 0;

    (void)blocksize;
    err = (uint8_t)HAL_SD_ReadBlocks_DMA(&SDCARD_Handler, (uint8_t *)buf, (uint32_t)sector, cnt);
    if (err == 0)
    {
        err = SD_WaitTransferDone(&SDCARD_Handler);
    }
    return err;
}

uint8_t SD_WriteBlocks_DMA(uint32_t *buf, uint64_t sector, uint32_t blocksize, uint32_t cnt)
{
    uint8_t err = 0;

    (void)blocksize;
    err = (uint8_t)HAL_SD_WriteBlocks_DMA(&SDCARD_Handler, (uint8_t *)buf, (uint32_t)sector, cnt);
    if (err == 0)
    {
        err = SD_WaitTransferDone(&SDCARD_Handler);
    }
    return err;
}

uint8_t SD_ReadDisk(uint8_t *buf, uint32_t sector, uint32_t cnt)
{
    uint8_t sta = SD_OK;
    long long lsector = sector;
    uint8_t n;

    if (SDCardInfo.CardType != STD_CAPACITY_SD_CARD_V1_1)
    {
        lsector <<= 9;
    }
    if (((uint32_t)buf % 4U) != 0U)
    {
        for (n = 0; n < cnt; n++)
        {
            sta = SD_ReadBlocks_DMA((uint32_t *)SDIO_DATA_BUFFER, lsector + 512 * n, 512, 1);
            memcpy(buf, SDIO_DATA_BUFFER, 512);
            buf += 512;
        }
    }
    else
    {
        sta = SD_ReadBlocks_DMA((uint32_t *)buf, lsector, 512, cnt);
    }
    return sta;
}

uint8_t SD_WriteDisk(uint8_t *buf, uint32_t sector, uint32_t cnt)
{
    uint8_t sta = SD_OK;
    long long lsector = sector;
    uint8_t n;

    if (SDCardInfo.CardType != STD_CAPACITY_SD_CARD_V1_1)
    {
        lsector <<= 9;
    }
    if (((uint32_t)buf % 4U) != 0U)
    {
        for (n = 0; n < cnt; n++)
        {
            memcpy(SDIO_DATA_BUFFER, buf, 512);
            sta = SD_WriteBlocks_DMA((uint32_t *)SDIO_DATA_BUFFER, lsector + 512 * n, 512, 1);
            buf += 512;
        }
    }
    else
    {
        sta = SD_WriteBlocks_DMA((uint32_t *)buf, lsector, 512, cnt);
    }
    return sta;
}

void SDIO_IRQHandler(void)
{
    HAL_SD_IRQHandler(&SDCARD_Handler);
}

void DMA2_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(SDCARD_Handler.hdmatx);
}

void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(SDCARD_Handler.hdmarx);
}

#else

uint8_t SD_ReadDisk(uint8_t *buf, uint32_t sector, uint32_t cnt)
{
    uint8_t sta = SD_OK;
    long long lsector = sector;
    uint8_t n;

    lsector <<= 9;
    taskENTER_CRITICAL();
    if (((uint32_t)buf % 4U) != 0U)
    {
        for (n = 0; n < cnt; n++)
        {
            sta = (uint8_t)HAL_SD_ReadBlocks(&SDCARD_Handler, (uint8_t *)SDIO_DATA_BUFFER,
                                             (uint32_t)(lsector + 512 * n), 1, SD_TIMEOUT);
            memcpy(buf, SDIO_DATA_BUFFER, 512);
            buf += 512;
        }
    }
    else
    {
        sta = (uint8_t)HAL_SD_ReadBlocks(&SDCARD_Handler, (uint8_t *)buf, (uint32_t)lsector, cnt, SD_TIMEOUT);
    }
    taskEXIT_CRITICAL();
    return sta;
}

uint8_t SD_WriteDisk(uint8_t *buf, uint32_t sector, uint32_t cnt)
{
    uint8_t sta = SD_OK;
    long long lsector = sector;
    uint8_t n;

    lsector <<= 9;
    taskENTER_CRITICAL();
    if (((uint32_t)buf % 4U) != 0U)
    {
        for (n = 0; n < cnt; n++)
        {
            memcpy(SDIO_DATA_BUFFER, buf, 512);
            sta = (uint8_t)HAL_SD_WriteBlocks(&SDCARD_Handler, (uint8_t *)SDIO_DATA_BUFFER,
                                              (uint32_t)(lsector + 512 * n), 1, SD_TIMEOUT);
            buf += 512;
        }
    }
    else
    {
        sta = (uint8_t)HAL_SD_WriteBlocks(&SDCARD_Handler, (uint8_t *)buf, (uint32_t)lsector, cnt, SD_TIMEOUT);
    }
    taskEXIT_CRITICAL();
    return sta;
}

#endif
