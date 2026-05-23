#include "sdio_utils.h"
#include "sdio_sdcard.h"
#include "FreeRTOS.h"

volatile SDIO_Dbg_t g_sdio_utils;

void show_sdcard_info(void)
{
    switch (SDCardInfo.CardType)
    {
    case STD_CAPACITY_SD_CARD_V1_1:
        g_sdio_utils.card_type_tag = 1;
        break;
    case STD_CAPACITY_SD_CARD_V2_0:
        g_sdio_utils.card_type_tag = 2;
        break;
    case HIGH_CAPACITY_SD_CARD:
        g_sdio_utils.card_type_tag = 3;
        break;
    case MULTIMEDIA_CARD:
        g_sdio_utils.card_type_tag = 4;
        break;
    default:
        g_sdio_utils.card_type_tag = 0;
        break;
    }

    g_sdio_utils.manufacturer_id = (uint8_t)((SDCARD_Handler.CID[0] >> 24) & 0xFFU);
    g_sdio_utils.rca = (uint16_t)SDCardInfo.RelCardAdd;
    g_sdio_utils.capacity_mb =
        (uint32_t)(((uint64_t)SDCardInfo.LogBlockNbr * SDCardInfo.LogBlockSize) >> 20);
    g_sdio_utils.block_size = SDCardInfo.LogBlockSize;
}

void sd_test_read(uint32_t secaddr, uint32_t seccnt)
{
    uint32_t i, len = seccnt * 512U;
    uint8_t *buf, sta;

    g_sdio_utils.read_sector = secaddr;
    buf = (uint8_t *)pvPortMalloc(len);
    if (buf == NULL)
    {
        g_sdio_utils.malloc_fail = 1;
        return;
    }

    sta = SD_ReadDisk(buf, secaddr, seccnt);
    g_sdio_utils.read_sta = sta;
    if (sta == 0)
    {
        g_sdio_utils.read_ok = 1;
        for (g_sdio_utils.read_byte_sum = 0, i = 0; i < len; i++)
        {
            g_sdio_utils.read_byte_sum += buf[i];
        }
    }
    else
    {
        g_sdio_utils.read_ok = 0;
    }
    vPortFree(buf);
}

void sd_test_write(uint32_t secaddr, uint32_t seccnt)
{
    uint32_t i, len = seccnt * 512U;
    uint8_t *buf, sta;

    buf = (uint8_t *)pvPortMalloc(len);
    if (buf == NULL)
    {
        g_sdio_utils.malloc_fail = 1;
        return;
    }

    for (i = 0; i < len; i++)
    {
        buf[i] = (uint8_t)(i * 3U);
    }

    sta = SD_WriteDisk(buf, secaddr, seccnt);
    g_sdio_utils.write_sta = sta;
    g_sdio_utils.write_ok = (sta == 0) ? 1U : 0U;
    vPortFree(buf);
}
