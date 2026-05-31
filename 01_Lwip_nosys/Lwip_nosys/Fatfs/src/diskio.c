/*-----------------------------------------------------------------------*/
/* FatFs disk I/O glue: SD card only (drive 0)                           */
/*-----------------------------------------------------------------------*/
#include <stdint.h>
#include "diskio.h"
#include "sdio_sdcard.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"

#define SD_CARD         0U
#define DISK_RD_RETRY   2U   /* 读失败后 SD_Recover 再试次数 */
#define DISK_RD_RECOVER_MS  50U

volatile Disk_Dbg_t g_disk_dbg;

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != SD_CARD) {
        return STA_NOINIT;
    }
    if (SDCardInfo.LogBlockNbr == 0U) {
        return STA_NOINIT;
    }
    return RES_OK;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != SD_CARD) {
        return STA_NOINIT;
    }
    /* main 里已 SD_Init 成功时跳过，避免二次 CMD55 */
    if (SDCardInfo.LogBlockNbr > 0U) {
        return RES_OK;
    }
    return (SD_Init() == SD_OK) ? RES_OK : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    uint8_t res;
    DRESULT dr;

    if (pdrv != SD_CARD || count == 0U) {
        g_disk_dbg.rd_fail_cnt++;
        g_disk_dbg.last_is_read = 1U;
        g_disk_dbg.last_sd_res = 0xFFU;
        g_disk_dbg.last_dresult = (uint8_t)RES_PARERR;
        g_disk_dbg.last_sector = sector;
        g_disk_dbg.last_count = (DWORD)count;
        g_disk_dbg.last_hal_error = SDCARD_Handler.ErrorCode;
        g_disk_dbg.last_sdio_sta = SDIO->STA;
        g_disk_dbg.last_hal_state = (BYTE)SDCARD_Handler.State;
        return RES_PARERR;
    }
    {
        uint8_t attempt;

        dr = RES_ERROR;
        for (attempt = 0U; attempt <= DISK_RD_RETRY; attempt++) {
            if (attempt > 0U) {
                (void)SD_Recover();
                vTaskDelay(pdMS_TO_TICKS(DISK_RD_RECOVER_MS));
            }
            res = SD_ReadDisk(buff, sector, count);
            if (res == SD_OK) {
                return RES_OK;
            }
        }
        g_disk_dbg.rd_fail_cnt++;
        g_disk_dbg.last_is_read = 1U;
        g_disk_dbg.last_sd_res = res;
        g_disk_dbg.last_dresult = (uint8_t)RES_ERROR;
        g_disk_dbg.last_sector = sector;
        g_disk_dbg.last_count = (DWORD)count;
        g_disk_dbg.last_hal_error = SDCARD_Handler.ErrorCode;
        g_disk_dbg.last_sdio_sta = SDIO->STA;
        g_disk_dbg.last_hal_state = (BYTE)SDCARD_Handler.State;
    }
    return dr;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    uint8_t res;
    DRESULT dr;

    if (pdrv != SD_CARD || count == 0U) {
        g_disk_dbg.wr_fail_cnt++;
        g_disk_dbg.last_is_read = 0U;
        g_disk_dbg.last_sd_res = 0xFFU;
        g_disk_dbg.last_dresult = (uint8_t)RES_PARERR;
        g_disk_dbg.last_sector = sector;
        g_disk_dbg.last_count = (DWORD)count;
        g_disk_dbg.last_hal_error = SDCARD_Handler.ErrorCode;
        g_disk_dbg.last_sdio_sta = SDIO->STA;
        g_disk_dbg.last_hal_state = (BYTE)SDCARD_Handler.State;
        return RES_PARERR;
    }
    res = SD_WriteDisk((uint8_t *)buff, sector, count);
    if (res == SD_OK) {
        dr = RES_OK;
    } else {
        g_disk_dbg.wr_fail_cnt++;
        g_disk_dbg.last_is_read = 0U;
        g_disk_dbg.last_sd_res = res;
        g_disk_dbg.last_dresult = (uint8_t)RES_ERROR;
        g_disk_dbg.last_sector = sector;
        g_disk_dbg.last_count = (DWORD)count;
        g_disk_dbg.last_hal_error = SDCARD_Handler.ErrorCode;
        g_disk_dbg.last_sdio_sta = SDIO->STA;
        g_disk_dbg.last_hal_state = (BYTE)SDCARD_Handler.State;
        dr = RES_ERROR;
    }
    return dr;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res;

    if (pdrv != SD_CARD) {
        return RES_PARERR;
    }

    switch (cmd) {
    case CTRL_SYNC:
        res = RES_OK;
        break;

    case GET_SECTOR_SIZE:
        *(DWORD *)buff = 512;
        res = RES_OK;
        break;

    case GET_BLOCK_SIZE:
        *(WORD *)buff = 1;
        res = RES_OK;
        break;

    case GET_SECTOR_COUNT:
        *(DWORD *)buff = SDCardInfo.LogBlockNbr;
        res = RES_OK;
        break;

    default:
        res = RES_PARERR;
        break;
    }
    return res;
}

DWORD get_fattime(void)
{
    return 0;
}

void *ff_memalloc(UINT size)
{
    if (size == 0U) {
        return NULL;
    }
    return pvPortMalloc((size_t)size);
}

void ff_memfree(void *mf)
{
    if (mf != NULL) {
        vPortFree(mf);
    }
}

#if _FS_REENTRANT

int ff_cre_syncobj(BYTE vol, _SYNC_t *sobj)
{
    SemaphoreHandle_t mtx;

    (void)vol;
    mtx = xSemaphoreCreateMutex();
    *sobj = mtx;
    return (mtx != NULL) ? 1 : 0;
}

int ff_del_syncobj(_SYNC_t sobj)
{
    vSemaphoreDelete((SemaphoreHandle_t)sobj);
    return 1;
}

int ff_req_grant(_SYNC_t sobj)
{
    return (int)(xSemaphoreTake((SemaphoreHandle_t)sobj, (TickType_t)_FS_TIMEOUT) == pdTRUE);
}

void ff_rel_grant(_SYNC_t sobj)
{
    (void)xSemaphoreGive((SemaphoreHandle_t)sobj);
}

#endif /* _FS_REENTRANT */
