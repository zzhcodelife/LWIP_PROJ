#ifndef __BSP_ETH_H__
#define __BSP_ETH_H__
#include "stm32f4xx.h"

/* Global Ethernet handle */
ETH_HandleTypeDef heth;

#if defined(__ICCARM__) /*!< IAR Compiler */
#pragma data_alignment = 4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef DMARxDscrTab[ETH_RXBUFNB] __ALIGN_END;
/* Ethernet Rx MA Descriptor */

#if defined(__ICCARM__) /*!< IAR Compiler */
#pragma data_alignment = 4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef DMATxDscrTab[ETH_TXBUFNB] __ALIGN_END;
/* Ethernet Tx DMA Descriptor */

#if defined(__ICCARM__) /*!< IAR Compiler */
#pragma data_alignment = 4
#endif
__ALIGN_BEGIN uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __ALIGN_END;
/* Ethernet Receive Buffer */

#if defined(__ICCARM__) /*!< IAR Compiler */
#pragma data_alignment = 4
#endif
__ALIGN_BEGIN uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __ALIGN_END;
/* Ethernet Transmit Buffer */

typedef struct
{
	ETH_TypeDef *Instance;			  /*!< Register base address */
	ETH_InitTypeDef Init;			  /*!< Ethernet Init Configuration */
	uint32_t LinkStatus;			  /*!< Ethernet link status */
	ETH_DMADescTypeDef *RxDesc;		  /*!< Rx descriptor to Get */
	ETH_DMADescTypeDef *TxDesc;		  /*!< Tx descriptor to Set */
	ETH_DMARxFrameInfos RxFrameInfos; /*!< last Rx frame infos */
	__IO HAL_ETH_StateTypeDef State;  /*!< ETH communication state */
	HAL_LockTypeDef Lock;			  /*!< ETH Lock */
} ETH_HandleTypeDef;
#endif	