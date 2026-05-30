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

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != SD_CARD) {     //只有SD卡一个设备
        return STA_NOINIT;
    }
    return RES_OK;
}

DSTATUS disk_initialize(BYTE pdrv)
{   
    uint8_t res = SD_Init();    
    return (res == SD_OK) ? RES_OK : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    uint8_t res;
    if (count == 0U) {
        return RES_PARERR;
    }
    res = SD_ReadDisk(buff,sector,count);	 
	while(res)      
	{
		SD_Init();	
		res = SD_ReadDisk(buff,sector,count);	
	}
    return (res == SD_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    uint8_t res;

    if (count == 0U) {
        return RES_PARERR;
    }
    res = SD_WriteDisk((uint8_t *)buff,sector,count);

    while(res)
    {
        SD_Init();
        res=SD_WriteDisk((uint8_t *)buff,sector,count);	
    }

    return (res == SD_OK) ? RES_OK : RES_ERROR;
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
        *(WORD*)buff = 1;
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
