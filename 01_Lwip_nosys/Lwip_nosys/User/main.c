/**
  *********************************************************************
  * @file    main.c
  * @brief   FreeRTOS + LwIP + 正点原子阿波罗 SD 卡测试
  *********************************************************************
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sdio/sdio_utils.h"

static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t SD_App_Task_Handle = NULL;

#define SD_APP_TASK_PERIOD_MS   (10U * 1000U)

static void AppTaskCreate(void);
static void SD_App_Task(void *pvParameters);
static void WIFI_PDN_INIT(void);
extern void TCPIP_Init(void);

int main(void)
{
  BaseType_t xReturn = pdPASS;

  BSP_Init();

  xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate,
                        (const char *)"AppTaskCreate",
                        (uint16_t)512,
                        (void *)NULL,
                        (UBaseType_t)1,
                        (TaskHandle_t *)&AppTaskCreate_Handle);

  if (pdPASS == xReturn)
  {
    vTaskStartScheduler();
  }
  else
  {
    return -1;
  }

  while (1)
  {
  }
}

static void AppTaskCreate(void)
{
  BaseType_t xReturn = pdPASS;

  TCPIP_Init();
  socketudp_init();
  taskENTER_CRITICAL();

  xReturn = xTaskCreate((TaskFunction_t)SD_App_Task,
                        (const char *)"SD_App_Task",
                        (uint16_t)2048,
                        (void *)NULL,
                        (UBaseType_t)2,
                        (TaskHandle_t *)&SD_App_Task_Handle);
  (void)xReturn;

  vTaskDelete(AppTaskCreate_Handle);
  taskEXIT_CRITICAL();
}

static void WIFI_PDN_INIT(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
}

/**
 * 对应正点原子实验39：Init 循环 -> 卡信息 -> 读写测试（无 LED/LCD/串口/KEY）
 * 调试 Watch：g_sdio_utils
 */
static void SD_App_Task(void *parameter)
{
  (void)parameter;

  WIFI_PDN_INIT();

  for (;;)
  {
    SDIO_Utils_RunOnce();
    vTaskDelay(pdMS_TO_TICKS(SD_APP_TASK_PERIOD_MS));
  }
}
