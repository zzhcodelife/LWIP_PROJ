/*-----------------------------------------------------------------------*/

/* FatFs：SD 初始化 + 格式化（可选）+ 挂载卷 "0:"                          */

/*-----------------------------------------------------------------------*/

#include "fatfs_utils.h"



#include <string.h>



#include "stm32f4xx_hal.h"

#include "sdio/sdio_sdcard.h"

#include "FreeRTOS.h"

#include "task.h"



volatile FatFs_Dbg_t g_fatfs;

volatile uint8_t     g_init_stage;



static FATFS s_fatfs;



static void fatfs_delay_ms(uint32_t ms)

{

    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {

        vTaskDelay(pdMS_TO_TICKS(ms));

    } else {

        HAL_Delay(ms);

    }

}



uint8_t fatfs_init(void)

{

    FRESULT fr;



    memset((void *)&g_fatfs, 0, sizeof(g_fatfs));



    while (SD_Init() != SD_OK) {

        fatfs_delay_ms(500);

    }



#if FATFS_RUN_MKFS

    fr = f_mount(&s_fatfs, FATFS_DRIVE, 0);

    if (fr != FR_OK) {

        g_fatfs.last_fr = fr;

        return 0U;

    }

    fr = f_mkfs(FATFS_DRIVE, 1, 0);

    if (fr != FR_OK) {

        g_fatfs.last_fr = fr;

        return 0U;

    }

    g_fatfs.mkfs_done = 1U;

#endif



    fr = f_mount(&s_fatfs, FATFS_DRIVE, 1);

    g_fatfs.last_fr = fr;

    if (fr != FR_OK) {

        return 0U;

    }



    g_fatfs.mounted = 1U;

    fr = f_mkdir(FATFS_DIR_ECUUPDATE);
    if (fr != FR_OK && fr != FR_EXIST) {
        g_fatfs.last_fr = fr;
        return 0U;
    }

    return 1U;

}

uint8_t fatfs_recover(void)
{
    FRESULT fr;

    if (g_fatfs.mounted != 0U) {
        (void)f_mount(NULL, FATFS_DRIVE, 0);
        g_fatfs.mounted = 0U;
    }

    if (SD_Recover() != SD_OK) {
        return 0U;
    }

    fatfs_delay_ms(20);

    fr = f_mount(&s_fatfs, FATFS_DRIVE, 1);
    g_fatfs.last_fr = fr;
    if (fr != FR_OK) {
        return 0U;
    }

    g_fatfs.mounted = 1U;
    return 1U;
}


