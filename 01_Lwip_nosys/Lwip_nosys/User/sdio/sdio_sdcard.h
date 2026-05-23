#ifndef __SDIO_SDCARD_H
#define __SDIO_SDCARD_H

#include "stm32f4xx_hal.h"

/* 0=轮询（FreeRTOS 任务内用临界区保护）  1=DMA+中断 */
#define SD_DMA_MODE                 0U

#define SD_TIMEOUT                  30000U

extern SD_HandleTypeDef       SDCARD_Handler;
extern HAL_SD_CardInfoTypeDef SDCardInfo;

uint8_t SD_Init(void);
uint8_t SD_GetCardInfo(HAL_SD_CardInfoTypeDef *cardinfo);
uint8_t SD_ReadDisk(uint8_t *buf, uint32_t sector, uint32_t cnt);
uint8_t SD_WriteDisk(uint8_t *buf, uint32_t sector, uint32_t cnt);

#endif
