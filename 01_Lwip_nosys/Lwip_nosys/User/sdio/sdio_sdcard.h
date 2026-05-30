#ifndef _SDMMC_SDCARD_H
#define _SDMMC_SDCARD_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#define SD_TIMEOUT   ((uint32_t)30000U)   /* ms，调试：30s 超时 */
#define SD_DMA_MODE  1U   /* 1：DMA模式，0：查询模式 */
#define SD_OK        0U

/* 正点原子示例里的卡类型判断（与 HAL CardType 数值兼容） */
#define STD_CAPACITY_SD_CARD_V1_1  CARD_SDSC
#define STD_CAPACITY_SD_CARD_V2_0  1U
#define HIGH_CAPACITY_SD_CARD      2U
#define MULTIMEDIA_CARD            CARD_SECURED

extern SD_HandleTypeDef       SDCARD_Handler;
extern HAL_SD_CardInfoTypeDef SDCardInfo;

uint8_t SD_Init(void);
uint8_t SD_GetCardInfo(HAL_SD_CardInfoTypeDef *cardinfo);
uint8_t SD_ReadDisk(uint8_t *buf, uint32_t sector, uint32_t cnt);
uint8_t SD_WriteDisk(uint8_t *buf, uint32_t sector, uint32_t cnt);
uint8_t SD_ReadBlocks_DMA(uint32_t *buf, uint64_t sector, uint32_t blocksize, uint32_t cnt);
uint8_t SD_WriteBlocks_DMA(uint32_t *buf, uint64_t sector, uint32_t blocksize, uint32_t cnt);

#endif
