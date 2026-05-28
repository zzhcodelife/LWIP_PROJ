/*-----------------------------------------------------------------------*/
/* FatFs 功能自测：挂载、读写、目录等（在 Test1_Task 中调用）              */
/*-----------------------------------------------------------------------*/
#include "fatfs_utils.h"
#include <string.h>

volatile FatFs_TestDbg_t g_fatfs_test;

static FATFS s_fatfs;
static const char s_test_payload[] = "FatFs R/W test on STM32F429.\r\n";

static void dbg_step(uint8_t step, FRESULT fr)
{
    g_fatfs_test.step = step;
    g_fatfs_test.last_fr = fr;
}

static uint8_t dbg_fail(uint8_t step, FRESULT fr)
{
    dbg_step(step, fr);
    g_fatfs_test.pass = 0U;
    g_fatfs_test.running = 0U;
    return 0U;
}

static FRESULT fatfs_mount_volume(void)
{
    FRESULT fr;

    fr = f_mount(&s_fatfs, FATFS_TEST_DRIVE, 1);
    if (fr == FR_NO_FILESYSTEM) {
#if FATFS_TEST_AUTO_MKFS
        fr = f_mkfs(FATFS_TEST_DRIVE, 1, 0);
        if (fr != FR_OK) {
            return fr;
        }
        g_fatfs_test.mkfs_done = 1U;
        fr = f_mount(&s_fatfs, FATFS_TEST_DRIVE, 1);
#endif
    }
    return fr;
}

static uint8_t test_getfree(void)
{
    FRESULT fr;
    DWORD free_clst;
    FATFS *fs;

    fr = f_getfree(FATFS_TEST_DRIVE, &free_clst, &fs);
    if (fr != FR_OK) {
        return dbg_fail(10U, fr);
    }
    g_fatfs_test.free_clusters = free_clst;
    g_fatfs_test.total_clusters = fs->n_fatent - 2U;
    return 1U;
}

static uint8_t test_mkdir(void)
{
    FRESULT fr;

    fr = f_mkdir(FATFS_TEST_DIR);
    if (fr == FR_EXIST) {
        fr = FR_OK;
    }
    if (fr != FR_OK) {
        return dbg_fail(20U, fr);
    }
    return 1U;
}

static uint8_t test_write_read(void)
{
    FRESULT fr;
    FIL fp;
    UINT bw;
    UINT br;
    char read_buf[64];

    g_fatfs_test.bytes_expected = (uint32_t)(sizeof(s_test_payload) - 1U);

    fr = f_open(&fp, FATFS_TEST_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        return dbg_fail(30U, fr);
    }

    fr = f_write(&fp, s_test_payload, (UINT)g_fatfs_test.bytes_expected, &bw);
    g_fatfs_test.bytes_written = bw;
    if (fr != FR_OK || bw != g_fatfs_test.bytes_expected) {
        f_close(&fp);
        return dbg_fail(31U, fr);
    }

    fr = f_sync(&fp);
    if (fr != FR_OK) {
        f_close(&fp);
        return dbg_fail(32U, fr);
    }

    fr = f_close(&fp);
    if (fr != FR_OK) {
        return dbg_fail(33U, fr);
    }

    fr = f_open(&fp, FATFS_TEST_FILE, FA_READ);
    if (fr != FR_OK) {
        return dbg_fail(34U, fr);
    }

    memset(read_buf, 0, sizeof(read_buf));
    fr = f_read(&fp, read_buf, sizeof(read_buf) - 1U, &br);
    g_fatfs_test.bytes_read = br;
    if (fr != FR_OK) {
        f_close(&fp);
        return dbg_fail(35U, fr);
    }

    fr = f_close(&fp);
    if (fr != FR_OK) {
        return dbg_fail(36U, fr);
    }

    g_fatfs_test.data_match =
        (strcmp(read_buf, s_test_payload) == 0) ? 1U : 0U;
    if (!g_fatfs_test.data_match) {
        return dbg_fail(37U, FR_OK);
    }

    return 1U;
}

static uint8_t test_stat(void)
{
    FRESULT fr;
    FILINFO fno;

    fr = f_stat(FATFS_TEST_FILE, &fno);
    if (fr != FR_OK) {
        return dbg_fail(40U, fr);
    }
    g_fatfs_test.stat_file_size = fno.fsize;
    return 1U;
}

static uint8_t test_readdir(void)
{
    FRESULT fr;
    DIR dir;
    FILINFO fno;
    uint32_t count = 0U;

    g_fatfs_test.last_dir_name[0] = '\0';

    fr = f_opendir(&dir, FATFS_TEST_DIR);
    if (fr != FR_OK) {
        return dbg_fail(50U, fr);
    }

    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) {
            break;
        }
        count++;
        strncpy(g_fatfs_test.last_dir_name, fno.fname,
                sizeof(g_fatfs_test.last_dir_name) - 1U);
        g_fatfs_test.last_dir_name[sizeof(g_fatfs_test.last_dir_name) - 1U] = '\0';
    }

    fr = f_closedir(&dir);
    if (fr != FR_OK) {
        return dbg_fail(51U, fr);
    }

    g_fatfs_test.dir_entries = count;
    if (count == 0U) {
        return dbg_fail(52U, FR_NO_FILE);
    }
    return 1U;
}

uint8_t fatfs_test_run(void)
{
    FRESULT fr;

    memset((void *)&g_fatfs_test, 0, sizeof(g_fatfs_test));
    g_fatfs_test.running = 1U;

#if FATFS_TEST_RUN_MKFS
    /* f_mkfs 要求 FatFs[vol] 已注册，须先 mount(opt=0) 再格式化 */
    fr = f_mount(&s_fatfs, FATFS_TEST_DRIVE, 0);
    if (fr != FR_OK) {
        return dbg_fail(0U, fr);
    }
    fr = f_mkfs(FATFS_TEST_DRIVE, 1, 0);
    if (fr != FR_OK) {
        return dbg_fail(1U, fr);
    }
    g_fatfs_test.mkfs_done = 1U;
#endif

    fr = fatfs_mount_volume();
    if (fr != FR_OK) {
        return dbg_fail(2U, fr);
    }
    g_fatfs_test.mounted = 1U;
    dbg_step(3U, FR_OK);

    if (!test_getfree()) {
        return 0U;
    }
    if (!test_mkdir()) {
        return 0U;
    }
    if (!test_write_read()) {
        return 0U;
    }
    if (!test_stat()) {
        return 0U;
    }
    if (!test_readdir()) {
        return 0U;
    }

    g_fatfs_test.pass = 1U;
    g_fatfs_test.running = 0U;
    dbg_step(99U, FR_OK);
    return 1U;
}
