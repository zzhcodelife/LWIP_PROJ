/**
  ******************************************************************************
  * @file    stm324x9i_eval_sd.c
  * @author  MCD Application Team
  * @brief   This file includes the uSD card driver mounted on STM324x9I-EVAL
  *          evaluation board.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */ 

/* File Info : -----------------------------------------------------------------
                                   User NOTES
1. How To use this driver:
--------------------------
   - This driver is used to drive the micro SD external card mounted on STM324x9I-EVAL 
     evaluation board.
   - This driver does not need a specific component driver for the micro SD device
     to be included with.

2. Driver description:
---------------------
  + Initialization steps:
     o Initialize the micro SD card using the BSP_SD_Init() function. This 
       function includes the MSP layer hardware resources initialization and the
       SDIO interface configuration to interface with the external micro SD. It 
       also includes the micro SD initialization sequence.
     o To check the SD card presence you can use the function BSP_SD_IsDetected() which 
       returns the detection status 
     o If SD presence detection interrupt mode is desired, you must configure the 
       SD detection interrupt mode by calling the function BSP_SD_ITConfig(). The interrupt 
       is generated as an external interrupt whenever the micro SD card is 
       plugged/unplugged in/from the evaluation board. The SD detection interrupt
       is handled by calling the function BSP_SD_DetectIT() which is called in the IRQ
       handler file, the user callback is implemented in the function BSP_SD_DetectCallback().
     o The function BSP_SD_GetCardInfo() is used to get the micro SD card information 
       which is stored in the structure "HAL_SD_CardInfoTypeDef".
  
     + Micro SD card operations
        o The micro SD card can be accessed with read/write block(s) operations once 
          it is ready for access. The access can be performed whether using the polling
          mode by calling the functions BSP_SD_ReadBlocks()/BSP_SD_WriteBlocks(), or by DMA 
          transfer using the functions BSP_SD_ReadBlocks_DMA()/BSP_SD_WriteBlocks_DMA()
        o The DMA transfer complete is used with interrupt mode. Once the SD transfer
          is complete, the SD interrupt is handled using the function BSP_SD_IRQHandler(),
          the DMA Tx/Rx transfer complete are handled using the functions
          BSP_SD_DMA_Tx_IRQHandler()/BSP_SD_DMA_Rx_IRQHandler(). The corresponding user callbacks 
          are implemented by the user at application level. 
        o The SD erase block(s) is performed using the function BSP_SD_Erase() with specifying
          the number of blocks to erase.
        o The SD runtime status is returned when calling the function BSP_SD_GetCardState().

 
------------------------------------------------------------------------------*/ 

/* Includes ------------------------------------------------------------------*/
#include "bsp_sdio_sd.h"
#include "stm32f4xx_hal_sd.h"


/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM324x9I_EVAL
  * @{
  */ 
  
/** @defgroup STM324x9I_EVAL_SD STM324x9I EVAL SD
  * @{
  */ 

/** @defgroup STM324x9I_EVAL_SD_Private_Variables SD Private Variables
  * @{
  */
SD_HandleTypeDef uSdHandle;

static DMA_HandleTypeDef s_sdDmaRxHandle;
static DMA_HandleTypeDef s_sdDmaTxHandle;
static uint8_t s_sdMspInited = 0U;

/**
  * @}
  */ 

/** @defgroup STM324x9I_EVAL_SD_Private_Functions SD Private Functions
  * @{
  */

void BSP_SD_ApplyTransferClock(void)
{
  uSdHandle.Init.ClockDiv = BSP_SD_CLKDIV_TRANS;
  SDIO_Init(uSdHandle.Instance, uSdHandle.Init);
}

uint8_t BSP_SD_PrepForTransfer(void)
{
  uint32_t tick = HAL_GetTick();

  BSP_SD_ApplyTransferClock();

  if (uSdHandle.State != HAL_SD_STATE_READY)
  {
    (void)HAL_SD_Abort(&uSdHandle);
    uSdHandle.State = HAL_SD_STATE_READY;
  }

  while (BSP_SD_GetCardState() != SD_TRANSFER_OK)
  {
    if ((HAL_GetTick() - tick) > 2000U)
    {
      return MSD_ERROR;
    }
  }
  return MSD_OK;
}

/**
  * @brief  Initializes the SD card device.
  * @retval SD status
  */
uint8_t BSP_SD_Init(void)
{ 
  uint8_t SD_state = MSD_OK;
  
  /* 已初始化且就绪时只刷新时钟，避免每轮 DeInit 把 DMA 句柄弄丢 */
  if (uSdHandle.State == HAL_SD_STATE_READY)
  {
    BSP_SD_ApplyTransferClock();
    return MSD_OK;
  }

  if (uSdHandle.State != HAL_SD_STATE_RESET)
  {
    s_sdMspInited = 0U;
    (void)HAL_SD_DeInit(&uSdHandle);
  }

#if BSP_SD_BOARD_ALIENTEK_APOLLO_F429
  if (BSP_SD_IsDetected() != SD_PRESENT)
  {
    return MSD_ERROR;
  }
#endif

  uSdHandle.Instance = SDIO;
  uSdHandle.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
  uSdHandle.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
  uSdHandle.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
  uSdHandle.Init.BusWide             = SDIO_BUS_WIDE_1B;
#if BSP_SD_USE_HW_FLOW_CTRL
  uSdHandle.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_ENABLE;
#else
  uSdHandle.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
#endif
  uSdHandle.Init.ClockDiv            = BSP_SD_CLKDIV_TRANS;

  if (HAL_SD_Init(&uSdHandle) != HAL_OK)
  {
    SD_state = MSD_ERROR;
  }
  else
  {
    BSP_SD_ApplyTransferClock();
  }
  
#if BSP_SD_USE_4BIT_BUS
  if (SD_state == MSD_OK)
  {
    if (HAL_SD_ConfigWideBusOperation(&uSdHandle, SDIO_BUS_WIDE_4B) != HAL_OK)
    {
      /* 4 线切换失败时退回 1 线，避免完全不可用 */
      uSdHandle.Init.BusWide = SDIO_BUS_WIDE_1B;
      BSP_SD_ApplyTransferClock();
    }
    else
    {
      uSdHandle.Init.BusWide = SDIO_BUS_WIDE_4B;
      BSP_SD_ApplyTransferClock();
    }
  }
#endif

  return SD_state;
}

uint8_t BSP_SD_IsDetected(void)
{
#if BSP_SD_BOARD_ALIENTEK_APOLLO_F429
  if (HAL_GPIO_ReadPin(BSP_SD_CD_GPIO_PORT, BSP_SD_CD_GPIO_PIN) ==
      BSP_SD_CD_INSERTED_LEVEL)
  {
    return SD_PRESENT;
  }
  return SD_NOT_PRESENT;
#else
  return SD_PRESENT;
#endif
}

///** @brief  SD detect IT treatment.
//  * @retval None
//  */
//void BSP_SD_DetectIT(void)
//{
//  /* Clear all pending bits */
//  BSP_IO_ITClear();
//  
//  /* To re-enable IT */
//  BSP_SD_ITConfig();
//  
//  /* SD detect IT callback */
//  BSP_SD_DetectCallback();
//}

/** @brief  SD detect IT detection callback
  * @retval None
  */
//__weak void BSP_SD_DetectCallback(void)
//{
//  /* NOTE: This function Should not be modified, when the callback is needed,
//     the BSP_SD_DetectCallback could be implemented in the user file
//  */ 
//}

/**
  * @brief  Reads block(s) from a specified address in an SD card, in polling mode.
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  ReadAddr: Address from where data is to be read
  * @param  NumOfBlocks: Number of SD blocks to read
  * @param  Timeout: Timeout for read operation
  * @retval SD status
  */
uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
  if(HAL_SD_ReadBlocks(&uSdHandle, (uint8_t *)pData, ReadAddr, NumOfBlocks, Timeout) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Writes block(s) to a specified address in an SD card, in polling mode. 
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  WriteAddr: Address from where data is to be written
  * @param  NumOfBlocks: Number of SD blocks to write
  * @param  Timeout: Timeout for write operation
  * @retval SD status
  */
uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
  if(HAL_SD_WriteBlocks(&uSdHandle, (uint8_t *)pData, WriteAddr, NumOfBlocks, Timeout) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Reads block(s) from a specified address in an SD card, in DMA mode.
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  ReadAddr: Address from where data is to be read
  * @param  NumOfBlocks: Number of SD blocks to read 
  * @retval SD status
  */
uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks)
{  
  /* Read block(s) in DMA transfer mode */
  if(HAL_SD_ReadBlocks_DMA(&uSdHandle, (uint8_t *)pData, ReadAddr, NumOfBlocks) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Writes block(s) to a specified address in an SD card, in DMA mode.
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  WriteAddr: Address from where data is to be written
  * @param  NumOfBlocks: Number of SD blocks to write 
  * @retval SD status
  */
uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks)
{ 
  /* Write block(s) in DMA transfer mode */
  if(HAL_SD_WriteBlocks_DMA(&uSdHandle, (uint8_t *)pData, WriteAddr, NumOfBlocks) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Erases the specified memory area of the given SD card. 
  * @param  StartAddr: Start byte address
  * @param  EndAddr: End byte address
  * @retval SD status
  */
uint8_t BSP_SD_Erase(uint32_t StartAddr, uint32_t EndAddr)
{
  if(HAL_SD_Erase(&uSdHandle, StartAddr, EndAddr) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Initializes the SD MSP.
  * @param  hsd: SD handle
  * @param  Params : pointer on additional configuration parameters, can be NULL.
  */
__weak void BSP_SD_MspInit(SD_HandleTypeDef *hsd, void *Params)
{
  GPIO_InitTypeDef GPIO_Init_Structure;

  if (s_sdMspInited != 0U)
  {
    __HAL_LINKDMA(hsd, hdmarx, s_sdDmaRxHandle);
    __HAL_LINKDMA(hsd, hdmatx, s_sdDmaTxHandle);
    return;
  }
  
  /* Enable SDIO clock */
  __HAL_RCC_SDIO_CLK_ENABLE();
  
  /* Enable DMA2 clocks */
  __DMAx_TxRx_CLK_ENABLE();

  /* Enable GPIOs clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
#if BSP_SD_BOARD_ALIENTEK_APOLLO_F429
  __HAL_RCC_GPIOA_CLK_ENABLE();
  {
    GPIO_InitTypeDef gpio_cd = {0};

    gpio_cd.Pin  = BSP_SD_CD_GPIO_PIN;
    gpio_cd.Mode = GPIO_MODE_INPUT;
    gpio_cd.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BSP_SD_CD_GPIO_PORT, &gpio_cd);
  }
#endif
  
  /* Common GPIO configuration */
  GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
  GPIO_Init_Structure.Pull      = GPIO_PULLUP;
  GPIO_Init_Structure.Speed     = GPIO_SPEED_HIGH;
  GPIO_Init_Structure.Alternate = GPIO_AF12_SDIO;
  
#if (BSP_SD_USE_4BIT_BUS != 0)
  GPIO_Init_Structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
  HAL_GPIO_Init(GPIOC, &GPIO_Init_Structure);
#else
  GPIO_Init_Structure.Pin = BSP_SD_1BIT_GPIO_PIN_D0 | BSP_SD_1BIT_GPIO_PIN_CLK;
  HAL_GPIO_Init(BSP_SD_1BIT_GPIO_PORT_D0, &GPIO_Init_Structure);
#endif

  GPIO_Init_Structure.Pin = BSP_SD_1BIT_GPIO_PIN_CMD;
  HAL_GPIO_Init(BSP_SD_1BIT_GPIO_PORT_CMD, &GPIO_Init_Structure);

  /* 优先级须 > configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY(5)，避免与 FreeRTOS 冲突 */
  HAL_NVIC_SetPriority(SDIO_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(SDIO_IRQn);
    
  /* Configure DMA Rx parameters */
  s_sdDmaRxHandle.Init.Channel             = SD_DMAx_Rx_CHANNEL;
  s_sdDmaRxHandle.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  s_sdDmaRxHandle.Init.PeriphInc           = DMA_PINC_DISABLE;
  s_sdDmaRxHandle.Init.MemInc              = DMA_MINC_ENABLE;
  s_sdDmaRxHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  s_sdDmaRxHandle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  s_sdDmaRxHandle.Init.Mode                = DMA_PFCTRL;
  s_sdDmaRxHandle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  s_sdDmaRxHandle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  s_sdDmaRxHandle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  s_sdDmaRxHandle.Init.MemBurst            = DMA_MBURST_INC4;
  s_sdDmaRxHandle.Init.PeriphBurst         = DMA_PBURST_INC4;
  s_sdDmaRxHandle.Instance                 = SD_DMAx_Rx_STREAM;
  __HAL_LINKDMA(hsd, hdmarx, s_sdDmaRxHandle);
  HAL_DMA_DeInit(&s_sdDmaRxHandle);
  HAL_DMA_Init(&s_sdDmaRxHandle);
  
  /* Configure DMA Tx parameters */
  s_sdDmaTxHandle.Init.Channel             = SD_DMAx_Tx_CHANNEL;
  s_sdDmaTxHandle.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  s_sdDmaTxHandle.Init.PeriphInc           = DMA_PINC_DISABLE;
  s_sdDmaTxHandle.Init.MemInc              = DMA_MINC_ENABLE;
  s_sdDmaTxHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  s_sdDmaTxHandle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  s_sdDmaTxHandle.Init.Mode                = DMA_PFCTRL;
  s_sdDmaTxHandle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  s_sdDmaTxHandle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  s_sdDmaTxHandle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  s_sdDmaTxHandle.Init.MemBurst            = DMA_MBURST_INC4;
  s_sdDmaTxHandle.Init.PeriphBurst         = DMA_PBURST_INC4;
  s_sdDmaTxHandle.Instance                 = SD_DMAx_Tx_STREAM;
  __HAL_LINKDMA(hsd, hdmatx, s_sdDmaTxHandle);
  HAL_DMA_DeInit(&s_sdDmaTxHandle);
  HAL_DMA_Init(&s_sdDmaTxHandle);
  
  HAL_NVIC_SetPriority(SD_DMAx_Rx_IRQn, 7, 0);
  HAL_NVIC_EnableIRQ(SD_DMAx_Rx_IRQn);
  HAL_NVIC_SetPriority(SD_DMAx_Tx_IRQn, 7, 0);
  HAL_NVIC_EnableIRQ(SD_DMAx_Tx_IRQn);

  s_sdMspInited = 1U;
}

/**
  * @brief  Gets the current SD card data status.
  * @retval Data transfer state.
  *          This value can be one of the following values:
  *            @arg  SD_TRANSFER_OK: No data transfer is acting
  *            @arg  SD_TRANSFER_BUSY: Data transfer is acting
  */
uint8_t BSP_SD_GetCardState(void)
{
  return((HAL_SD_GetCardState(&uSdHandle) == HAL_SD_CARD_TRANSFER ) ? SD_TRANSFER_OK : SD_TRANSFER_BUSY);
}
  

/**
  * @brief  Get SD information about specific SD card.
  * @param  CardInfo: Pointer to HAL_SD_CardInfoTypedef structure
  * @retval None 
  */
void BSP_SD_GetCardInfo(HAL_SD_CardInfoTypeDef *CardInfo)
{
  /* Get SD card Information */
  HAL_SD_GetCardInfo(&uSdHandle, CardInfo);
}

/**
  * @brief SD Abort callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_AbortCallback(SD_HandleTypeDef *hsd)
{
  BSP_SD_AbortCallback();
}

/**
  * @brief Tx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
  BSP_SD_WriteCpltCallback();
}

/**
  * @brief Rx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
  BSP_SD_ReadCpltCallback();
}

/**
  * @brief BSP SD Abort callbacks
  * @retval None
  */
__weak void BSP_SD_AbortCallback(void)
{

}

/**
  * @brief BSP Tx Transfer completed callbacks
  * @retval None
  */
__weak void BSP_SD_WriteCpltCallback(void)
{

}

/**
  * @brief BSP Rx Transfer completed callbacks
  * @retval None
  */
__weak void BSP_SD_ReadCpltCallback(void)
{

}

/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */
 
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
