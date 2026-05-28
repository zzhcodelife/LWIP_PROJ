/*-----------------------------------------------------------------------*/
/* FatFs disk I/O glue: SD card only (drive 0)                           */
/*-----------------------------------------------------------------------*/
#include <stdint.h>
#include "diskio.h"
#include "sdio_sdcard.h"
#include "fatfs_utils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"

#define SD_CARD       0U
#define DISK_IO_RETRY 3U

static uint8_t disk_try_rw(uint8_t (*fn)(uint8_t *, uint32_t, uint32_t),
                             uint8_t *buf, uint32_t sector, uint32_t count)
{
    uint8_t res;
    uint8_t retry;

    res = fn(buf, sector, count);
    for (retry = 0U; res != SD_OK && retry < DISK_IO_RETRY; retry++) {
        if (SD_Init() != SD_OK) {
            break;
        }
        res = fn(buf, sector, count);
    }

    if (res != SD_OK) {
        g_fatfs_test.disk_last_sector = sector;
        g_fatfs_test.disk_last_sta = res;
        g_fatfs_test.disk_last_hal_err = (uint32_t)SDCARD_Handler.ErrorCode;
    }
    return res;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != SD_CARD) {
        return STA_NOINIT;
    }
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    uint8_t res;

    if (pdrv != SD_CARD) {
        return STA_NOINIT;
    }

    res = SD_Init();
    if (res == SD_OK) {
        g_fatfs_test.sd_sector_count = SDCardInfo.LogBlockNbr;
    }
    return (res == SD_OK) ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    uint8_t res;

    if (pdrv != SD_CARD || buff == NULL || count == 0U) {
        return RES_PARERR;
    }

    res = disk_try_rw(SD_ReadDisk, buff, sector, count);
    return (res == SD_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    uint8_t res;

    if (pdrv != SD_CARD || buff == NULL || count == 0U) {
        return RES_PARERR;
    }

    res = disk_try_rw(SD_WriteDisk, (uint8_t *)buff, sector, count);
    return (res == SD_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != SD_CARD || buff == NULL) {
        return RES_PARERR;
    }

    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;

    case GET_BLOCK_SIZE:
        *(WORD *)buff = 1;
        return RES_OK;

    case GET_SECTOR_COUNT:
        if (SDCardInfo.LogBlockNbr < 128U) {
            return RES_ERROR;
        }
        *(DWORD *)buff = SDCardInfo.LogBlockNbr;
        return RES_OK;

    default:
        return RES_PARERR;
    }
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
