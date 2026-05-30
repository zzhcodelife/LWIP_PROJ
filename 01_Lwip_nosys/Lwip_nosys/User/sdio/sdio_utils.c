#include "sdio_utils.h"
#include "sdio_sdcard.h"
#include "FreeRTOS.h"

volatile SDIO_Dbg_t g_sdio_utils;

void show_sdcard_info(void)
{
    if (SDCardInfo.CardType == CARD_SDSC)
    {
        g_sdio_utils.card_type_tag = (SDCardInfo.CardVersion == 0U) ? 1U : 2U;
    }
    else if (SDCardInfo.CardType == CARD_SDHC_SDXC)
    {
        g_sdio_utils.card_type_tag = 3U;
    }
    else if (SDCardInfo.CardType == CARD_SECURED)
    {
        g_sdio_utils.card_type_tag = 4U;
    }
    else
    {
        g_sdio_utils.card_type_tag = 0U;
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

uint8_t sd_test_rw_verify(uint32_t secaddr, uint32_t seccnt)
{
    uint32_t i, len = seccnt * 512U;
    uint8_t *buf;
    uint8_t sta;

    g_sdio_utils.test_sector = secaddr;
    g_sdio_utils.compare_ok = 0U;
    g_sdio_utils.compare_fail_index = 0xFFFFFFFFU;

    sd_test_write(secaddr, seccnt);
    if (g_sdio_utils.write_ok == 0U)
    {
        return 0U;
    }

    buf = (uint8_t *)pvPortMalloc(len);
    if (buf == NULL)
    {
        g_sdio_utils.malloc_fail = 1U;
        return 0U;
    }

    sta = SD_ReadDisk(buf, secaddr, seccnt);
    g_sdio_utils.read_sta = sta;
    if (sta != 0U)
    {
        g_sdio_utils.read_ok = 0U;
        vPortFree(buf);
        return 0U;
    }

    g_sdio_utils.read_ok = 1U;
    g_sdio_utils.read_byte_sum = 0U;
    for (i = 0U; i < len; i++)
    {
        uint8_t expect = (uint8_t)(i * 3U);

        g_sdio_utils.read_byte_sum += buf[i];
        if (buf[i] != expect)
        {
            g_sdio_utils.compare_fail_index = i;
            g_sdio_utils.compare_fail_expect = expect;
            g_sdio_utils.compare_fail_actual = buf[i];
            vPortFree(buf);
            return 0U;
        }
    }

    g_sdio_utils.compare_ok = 1U;
    vPortFree(buf);
    return 1U;
}

void sd_test_rw_verify_dual(void)
{
    g_sdio_utils.verify_ok_4096 = sd_test_rw_verify(4096U, 1U);
    g_sdio_utils.verify_ok_0 = sd_test_rw_verify(0U, 1U);
}
