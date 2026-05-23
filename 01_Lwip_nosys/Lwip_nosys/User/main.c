/**
  *********************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2019-xx-xx
  * @brief   FreeRTOS V9.0.0 + STM32 LwIP
  *********************************************************************
  * @attention
  *
  * 实验平台:野火  STM32全系列开发板 
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  **********************************************************************
  */ 
 
/*
*************************************************************************
*                             包含的头文件
*************************************************************************
*/ 
#include "main.h"
/* FreeRTOS头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/**************************** 任务句柄 ********************************/
/* 
 * 任务句柄是一个指针，用于指向一个任务，当任务创建好之后，它就具有了一个任务句柄
 * 以后我们要想操作这个任务都需要通过这个任务句柄，如果是自身的任务操作自己，那么
 * 这个句柄可以为NULL。
 */
static TaskHandle_t AppTaskCreate_Handle = NULL;/* 创建任务句柄 */
static TaskHandle_t Test1_Task_Handle = NULL;/* LED任务句柄 */
static TaskHandle_t Test2_Task_Handle = NULL;/* KEY任务句柄 */

/********************************** 内核对象句柄 *********************************/
/*
 * 信号量，消息队列，事件标志组，软件定时器这些都属于内核的对象，要想使用这些内核
 * 对象，必须先创建，创建成功之后会返回一个相应的句柄。实际上就是一个指针，后续我
 * 们就可以通过这个句柄操作这些内核对象。
 *
 * 内核对象说白了就是一种全局的数据结构，通过这些数据结构我们可以实现任务间的通信，
 * 任务间的事件同步等各种功能。至于这些功能的实现我们是通过调用这些内核对象的函数
 * 来完成的
 * 
 */

/******************************* 全局变量声明 ************************************/
/*
 * 当我们在写应用程序的时候，可能需要用到一些全局变量。
 */


/******************************* 宏定义 ************************************/
/*
 * 当我们在写应用程序的时候，可能需要用到一些宏定义。
 */


/*
*************************************************************************
*                             函数声明
*************************************************************************
*/
static void AppTaskCreate(void);/* 用于创建任务 */

static void Test1_Task(void* pvParameters);/* Test1_Task任务实现 */
static void Test2_Task(void* pvParameters);/* Test2_Task任务实现 */

extern void TCPIP_Init(void);

/*****************************************************************
  * @brief  主函数
  * @param  无
  * @retval 无
  * @note   第一步：开发板硬件初始化 
            第二步：创建APP应用任务
            第三步：启动FreeRTOS，开始多任务调度
  ****************************************************************/
int main(void)
{	
  BaseType_t xReturn = pdPASS;/* 定义一个创建信息返回值，默认为pdPASS */
  
  /* 开发板硬件初始化 */
  BSP_Init();
  
  
  
//  tcpecho_init();
  
  /* 创建AppTaskCreate任务 */
  xReturn = xTaskCreate((TaskFunction_t )AppTaskCreate,  /* 任务入口函数 */
                        (const char*    )"AppTaskCreate",/* 任务名字 */
                        (uint16_t       )512,  /* 任务栈大小 */
                        (void*          )NULL,/* 任务入口函数参数 */
                        (UBaseType_t    )1, /* 任务的优先级 */
                        (TaskHandle_t*  )&AppTaskCreate_Handle);/* 任务控制块指针 */ 
  /* 启动任务调度 */           
  if(pdPASS == xReturn)
    vTaskStartScheduler();   /* 启动任务，开启调度 */
  else
    return -1;  
  
  while(1);   /* 正常不会执行到这里 */    
}


/***********************************************************************
  * @ 函数名  ： AppTaskCreate
  * @ 功能说明： 为了方便管理，所有的任务创建函数都放在这个函数里面
  * @ 参数    ： 无  
  * @ 返回值  ： 无
  **********************************************************************/
static void AppTaskCreate(void)
{
  BaseType_t xReturn = pdPASS;/* 定义一个创建信息返回值，默认为pdPASS */
  TCPIP_Init();
  //client_init();
  //tcpecho_init();
  //udpecho_init();
  //socket_client_init();
  //socketserver_init();
  socketudp_init();
  taskENTER_CRITICAL();           //进入临界区

  /* 创建Test1_Task任务 */
  xReturn = xTaskCreate((TaskFunction_t )Test1_Task, /* 任务入口函数 */
                        (const char*    )"Test1_Task",/* 任务名字 */
                        (uint16_t       )512,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )1,	    /* 任务的优先级 */
                        (TaskHandle_t*  )&Test1_Task_Handle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    //printf("Create Test1_Task sucess...\r\n");
  
  /* 创建Test2_Task任务 */
  xReturn = xTaskCreate((TaskFunction_t )Test2_Task,  /* 任务入口函数 */
                        (const char*    )"Test2_Task",/* 任务名字 */
                        (uint16_t       )512,  /* 任务栈大小 */
                        (void*          )NULL,/* 任务入口函数参数 */
                        (UBaseType_t    )2, /* 任务的优先级 */
                        (TaskHandle_t*  )&Test2_Task_Handle);/* 任务控制块指针 */ 
  if(pdPASS == xReturn)
//    printf("Create Test2_Task sucess...\n\n");
  
  vTaskDelete(AppTaskCreate_Handle); //删除AppTaskCreate任务
  
  taskEXIT_CRITICAL();            //退出临界区
}



/**********************************************************************
  * @ 函数名  ： Test1_Task
  * @ 功能说明： Test1_Task任务主体
  * @ 参数    ：   
  * @ 返回值  ： 无
  ********************************************************************/
static void Test1_Task(void* parameter)
{	
  while (1)
  {
//    PRINT_DEBUG("LED1_TOGGLE\n");
    vTaskDelay(1000);/* 延时1000个tick */
  }
}

/**********************************************************************
  * @ 函数名  ： Test2_Task
  * @ 功能说明： Test2_Task任务主体
  * @ 参数    ：   
  * @ 返回值  ： 无
  ********************************************************************/
static void Test2_Task(void* parameter)
{	 
  while (1)
  {
//    PRINT_DEBUG("LED2_TOGGLE\n");
    vTaskDelay(2000);/* 延时2000个tick */
  }
}



/********************************END OF FILE****************************/

/**
	**************************************************************
	* Description : 初始化WiFi模块使能引脚，并禁用WiFi模块
	* Argument(s) : none.
	* Return(s)   : none.
	**************************************************************
	*/
  static void WIFI_PDN_INIT(void)
  {
    /*定义一个GPIO_InitTypeDef类型的结构体*/
    GPIO_InitTypeDef GPIO_InitStruct;
    /*使能引脚时钟*/	
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /*选择要控制的GPIO引脚*/															   
    GPIO_InitStruct.Pin = GPIO_PIN_13;	
    /*设置引脚的输出类型为推挽输出*/
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;      
    /*设置引脚为上拉模式*/
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    /*设置引脚速率为高速 */   
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST; 
    /*调用库函数，使用上面配置的GPIO_InitStructure初始化GPIO*/
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);	
    /*禁用WiFi模块*/
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_13,GPIO_PIN_RESET);  
  }
  
  /**
    * @brief  主函数
    * @param  无
    * @retval 无
    */
  int main(void)
  {	
    /* 配置系统时钟为216 MHz */
     SystemClock_Config();
    /*禁用WiFi模块*/
    WIFI_PDN_INIT();
    
    /* 初始化LED灯 */
     LED_GPIO_Config();
    LED_BLUE;	
    /* 初始化独立按键 */
    Key_GPIO_Config();
    
    /*初始化USART1*/
    DEBUG_USART_Config();
    
//    printf("\r\n欢迎使用野火  STM32 F429 开发板。\r\n");
    
//    printf("在开始进行SD卡基本测试前，请给开发板插入32G以内的SD卡\r\n");
//    printf("本程序会对SD卡进行 非文件系统 方式读写，会删除SD卡的文件系统\r\n");
//    printf("实验后可通过电脑格式化或使用SD卡文件系统的例程恢复SD卡文件系统\r\n");
//    printf("\r\n 但sd卡内的原文件不可恢复，实验前务必备份SD卡内的原文件！！！\r\n");
    
//    printf("\r\n 若已确认，请按开发板的KEY1按键，开始SD卡测试实验....\r\n");
    
    /* Infinite loop */
    while (1)
    {	
      /*按下按键开始进行SD卡读写实验，会损坏SD卡原文件*/
      if(	Key_Scan(KEY1_GPIO_PORT,KEY1_PIN) == KEY_ON)
      {
//        printf("\r\n开始进行SD卡读写实验\r\n");
        SD_Test();			
      }
    } 
  }
  
  /**
    * @brief  系统时钟配置 
    *            System Clock source            = PLL (HSE)
    *            SYSCLK(Hz)                     = 180000000
    *            HCLK(Hz)                       = 180000000
    *            AHB Prescaler                  = 1
    *            APB1 Prescaler                 = 4
    *            APB2 Prescaler                 = 2
    *            HSE Frequency(Hz)              = 12000000
    *            PLL_M                          = 25
    *            PLL_N                          = 360
    *            PLL_P                          = 2
    *            PLL_Q                          = 4
    *            VDD(V)                         = 3.3
    *            Main regulator output voltage  = Scale1 mode
    *            Flash Latency(WS)              = 5
    * @param  无
    * @retval 无
    */
  static void SystemClock_Config(void)
  {
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    HAL_StatusTypeDef ret = HAL_OK;
    
     /* 使能HSE，配置HSE为PLL的时钟源，配置PLL的各种分频因子M N P Q 
      * PLLCLK = HSE/M*N/P = 12M / 25 *360 / 2 = 180M
      */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 360;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    if( ret != HAL_OK)
    {
      while(1) {}
    }
  
    /* 激活 OverDrive 模式以达到180M频率 */
    ret =HAL_PWREx_EnableOverDrive();
     if( ret != HAL_OK)
    {
      while(1) {}
    }
   
    /* 选择PLLCLK作为SYSCLK，并配置 HCLK, PCLK1 and PCLK2 的时钟分频因子 
     * SYSCLK = PLLCLK     = 180M
     * HCLK   = SYSCLK / 1 = 180M
     * PCLK2  = SYSCLK / 2 = 90M
     * PCLK1  = SYSCLK / 4 = 45M
     */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    
    ret =HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
    
     if( ret != HAL_OK)
    {
      while(1) {}
    }
  }
  
  
  /*********************************************END OF FILE**********************/