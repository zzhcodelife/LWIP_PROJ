/*-----------------------------------------------------------------------*/
/* FatFs disk I/O glue: SD card only (drive 0)                           */
/*-----------------------------------------------------------------------*/
#include <stdint.h>
#include "diskio.h"
#include "sdio_sdcard.h"
#include "FreeRTOS.h"
#include "task.h"

#define SD_CARD  0U

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
    return (res == SD_OK) ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    uint8_t res;

    if (pdrv != SD_CARD || buff == NULL || count == 0U) {
        return RES_PARERR;
    }

    res = SD_ReadDisk(buff, sector, count);
    while (res != SD_OK) {
        if (SD_Init() != SD_OK) {
            break;
        }
        res = SD_ReadDisk(buff, sector, count);
    }

    return (res == SD_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    uint8_t res;

    if (pdrv != SD_CARD || buff == NULL || count == 0U) {
        return RES_PARERR;
    }

    res = SD_WriteDisk((uint8_t *)buff, sector, count);
    while (res != SD_OK) {
        if (SD_Init() != SD_OK) {
            break;
        }
        res = SD_WriteDisk((uint8_t *)buff, sector, count);
    }

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
