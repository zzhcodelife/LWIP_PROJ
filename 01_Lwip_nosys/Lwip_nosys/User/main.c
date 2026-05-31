/**
  *********************************************************************
  * @file    main.c
  * @brief   FreeRTOS + LwIP + TFTP
  **********************************************************************
  */

#include "main.h"
#include "fatfs_utils.h"
#include "FreeRTOS.h"
#include "task.h"

static TaskHandle_t AppTaskCreate_Handle = NULL;

static void AppTaskCreate(void *arg);

extern void TCPIP_Init(void);

int main(void)
{
  BaseType_t xReturn;

  BSP_Init();

  xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate,
                        "AppTaskCreate",
                        2048,
                        NULL,
                        2,
                        &AppTaskCreate_Handle);
  if (xReturn != pdPASS) {
    return -1;
  }

  vTaskStartScheduler();

  while (1) {
  }
}

static void AppTaskCreate(void *arg)
{
  (void)arg;

  TCPIP_Init();
  g_init_stage = INIT_STAGE_TCPIP;

  (void)fatfs_init();
  g_init_stage = INIT_STAGE_FATFS;

  socketudp_init();
  g_init_stage = INIT_STAGE_TFTP;

  vTaskDelete(AppTaskCreate_Handle);
}

/********************************END OF FILE****************************/
