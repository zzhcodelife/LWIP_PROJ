/**
 * @file tftp_fs.c
 */
#include "tftp_fs.h"

#include <string.h>
#include <stdio.h>

#include "fatfs_utils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define TFTP_FS_STACK_WORDS  1536U
#define TFTP_FS_TASK_PRIO    5U
#define TFTP_FS_RPC_TIMEOUT      pdMS_TO_TICKS(35000) /* 与 SD_TIMEOUT 同量级 */
#define TFTP_FS_WRITE_RETRY_MAX  3U
#define TFTP_FS_WRITE_RETRY_MS   20U
#define TFTP_FS_READ_RETRY_MAX   2U    /* f_read 失败：恢复 SD+重挂载+seek 后再读 */
#define TFTP_FS_READ_RECOVER_MS  100U

typedef enum {
    FS_CMD_OPEN_READ = 0,
    FS_CMD_OPEN_WRITE,
    FS_CMD_READ,
    FS_CMD_WRITE,
    FS_CMD_CLOSE,
} fs_cmd_t;

typedef struct {
    fs_cmd_t cmd;
    char     path[TFTP_FS_PATH_MAX];
    uint16_t len;
    uint8_t  data[512];
} fs_request_t;

typedef struct {
    FRESULT  fr;
    uint16_t len;
    uint8_t  data[512];
} fs_response_t;

static QueueHandle_t s_req_q;
static QueueHandle_t s_rsp_q;
static TaskHandle_t  s_task;

static FIL     s_fil;
static uint8_t s_open;
static char    s_open_path[TFTP_FS_PATH_MAX];

static void fs_force_close(void)
{
    if (s_open) {
        (void)f_sync(&s_fil);
        (void)f_close(&s_fil);
        s_open = 0U;
    }
}

/* 读失败：SD 恢复 + 按原偏移重新 open */
static FRESULT fs_recover_reopen_read(DWORD pos)
{
    FRESULT fr;

    fs_force_close();
    if (fatfs_recover() == 0U) {
        return FR_DISK_ERR;
    }
    if (s_open_path[0] == '\0') {
        return FR_NOT_READY;
    }
    fr = f_open(&s_fil, s_open_path, FA_READ);
    if (fr != FR_OK) {
        return fr;
    }
    s_open = 1U;
    if (pos > 0U) {
        fr = f_lseek(&s_fil, pos);
        if (fr != FR_OK) {
            fs_force_close();
            return fr;
        }
    }
    return FR_OK;
}

static void fs_build_path(char *dst, const char *tftp_name)
{
    size_t i = 0U;
    const char *p = tftp_name;

    if (p[0] == '/' || p[0] == '\\') {
        p++;
    }

    i = (size_t)snprintf(dst, TFTP_FS_PATH_MAX, FATFS_DRIVE "/");
    if (i >= TFTP_FS_PATH_MAX) {
        return;
    }

    while (*p != '\0' && i + 1U < TFTP_FS_PATH_MAX) {
        char c = *p++;

        if (c == '\\') {
            c = '/';
        }
        dst[i++] = c;
    }
    dst[i] = '\0';
}

static void fs_worker_task(void *arg)
{
    fs_request_t req;
    fs_response_t rsp;

    (void)arg;

    for (;;) {
        if (xQueueReceive(s_req_q, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        rsp.fr  = FR_OK;
        rsp.len = 0U;

        switch (req.cmd) {
        case FS_CMD_OPEN_READ:
        {
            char fat_path[TFTP_FS_PATH_MAX];

            fs_force_close();
            fs_build_path(fat_path, req.path);
            rsp.fr = f_open(&s_fil, fat_path, FA_READ);
            g_tftp_dbg.last_fs_fr = (uint8_t)rsp.fr;
            strncpy((char *)g_tftp_dbg.last_fs_path, fat_path,
                    sizeof(g_tftp_dbg.last_fs_path) - 1U);
            g_tftp_dbg.last_fs_path[sizeof(g_tftp_dbg.last_fs_path) - 1U] = '\0';
            strncpy(s_open_path, fat_path, sizeof(s_open_path) - 1U);
            s_open_path[sizeof(s_open_path) - 1U] = '\0';
            s_open = (rsp.fr == FR_OK) ? 1U : 0U;
            break;
        }

        case FS_CMD_OPEN_WRITE:
        {
            char fat_path[TFTP_FS_PATH_MAX];

            fs_force_close();
            fs_build_path(fat_path, req.path);
            rsp.fr = f_open(&s_fil, fat_path, FA_CREATE_ALWAYS | FA_WRITE);
            g_tftp_dbg.last_fs_fr = (uint8_t)rsp.fr;
            strncpy((char *)g_tftp_dbg.last_fs_path, fat_path,
                    sizeof(g_tftp_dbg.last_fs_path) - 1U);
            g_tftp_dbg.last_fs_path[sizeof(g_tftp_dbg.last_fs_path) - 1U] = '\0';
            strncpy(s_open_path, fat_path, sizeof(s_open_path) - 1U);
            s_open_path[sizeof(s_open_path) - 1U] = '\0';
            s_open = (rsp.fr == FR_OK) ? 1U : 0U;
            break;
        }

        case FS_CMD_READ:
            if (!s_open) {
                rsp.fr = FR_NOT_READY;
                break;
            }
            {
                UINT br = 0U;
                uint8_t attempt;
                DWORD pos;

                pos = f_tell(&s_fil);
                rsp.fr = FR_DISK_ERR;
                for (attempt = 0U; attempt < TFTP_FS_READ_RETRY_MAX; attempt++) {
                    if (attempt > 0U) {
                        vTaskDelay(pdMS_TO_TICKS(TFTP_FS_READ_RECOVER_MS));
                        g_tftp_dbg.err_step = 46U; /* 已做 fatfs_recover */
                        rsp.fr = fs_recover_reopen_read(pos);
                        if (rsp.fr != FR_OK) {
                            break;
                        }
                    }
                    br = 0U;
                    g_tftp_dbg.fs_step = 40U;
                    rsp.fr = f_read(&s_fil, rsp.data, req.len, &br);
                    rsp.len = (uint16_t)br;
                    /* FR_OK 且 br<=req.len 均合法：含文件尾不足 512B、含整 512 对齐文件的 EOF(br=0) */
                    if (rsp.fr == FR_OK) {
                        break;
                    }
                    if (rsp.fr != FR_DISK_ERR) {
                        break;
                    }
                }
                g_tftp_dbg.last_fs_fr = (uint8_t)rsp.fr;
                g_tftp_dbg.fs_step = 41U;
                if (rsp.fr != FR_OK) {
                    g_tftp_dbg.err_step = 45U;
                }
            }
            break;

        case FS_CMD_WRITE:
            if (!s_open) {
                rsp.fr = FR_NOT_READY;
                break;
            }
            {
                UINT bw = 0U;
                uint8_t attempt;

                rsp.fr = FR_DISK_ERR;
                for (attempt = 0U; attempt < TFTP_FS_WRITE_RETRY_MAX; attempt++) {
                    if (attempt > 0U) {
                        vTaskDelay(pdMS_TO_TICKS(TFTP_FS_WRITE_RETRY_MS));
                    }
                    bw = 0U;
                    rsp.fr = f_write(&s_fil, req.data, req.len, &bw);
                    rsp.len = (uint16_t)bw;
                    if (rsp.fr == FR_OK && rsp.len == req.len) {
                        break;
                    }
                    if (rsp.fr == FR_OK && rsp.len != req.len) {
                        rsp.fr = FR_DISK_ERR;
                    }
                    /* 仅对瞬时磁盘错误重试；其它错误直接退出 */
                    if (rsp.fr != FR_DISK_ERR) {
                        break;
                    }
                }
                g_tftp_dbg.last_fs_fr = (uint8_t)rsp.fr;
                if (rsp.fr != FR_OK) {
                    fs_force_close();
                }
            }
            break;

        case FS_CMD_CLOSE:
            fs_force_close();
            rsp.fr = FR_OK;
            break;

        default:
            rsp.fr = FR_INVALID_PARAMETER;
            break;
        }

        (void)xQueueSend(s_rsp_q, &rsp, portMAX_DELAY);
    }
}

static uint8_t fs_rpc_call(fs_request_t *req, fs_response_t *rsp)
{
    if (s_req_q == NULL || s_rsp_q == NULL) {
        return 0U;
    }
    if (xQueueSend(s_req_q, req, TFTP_FS_RPC_TIMEOUT) != pdTRUE) {
        return 0U;
    }
    if (xQueueReceive(s_rsp_q, rsp, TFTP_FS_RPC_TIMEOUT) != pdTRUE) {
        return 0U;
    }
    return 1U;
}

void tftp_fs_worker_start(void)
{
    if (s_task != NULL) {
        return;
    }

    s_req_q = xQueueCreate(1, sizeof(fs_request_t));
    s_rsp_q = xQueueCreate(1, sizeof(fs_response_t));
    if (s_req_q == NULL || s_rsp_q == NULL) {
        return;
    }

    if (xTaskCreate(fs_worker_task, "tftp_fs",
                    TFTP_FS_STACK_WORDS, NULL, TFTP_FS_TASK_PRIO, &s_task) == pdPASS) {
        g_tftp_dbg.fs_worker_ok = 1U;
    }
}

FRESULT tftp_fs_open_read(const char *tftp_name)
{
    fs_request_t req;
    fs_response_t rsp;

    memset(&req, 0, sizeof(req));
    req.cmd = FS_CMD_OPEN_READ;
    strncpy(req.path, tftp_name, sizeof(req.path) - 1U);

    if (!fs_rpc_call(&req, &rsp)) {
        return FR_TIMEOUT;
    }
    return rsp.fr;
}

FRESULT tftp_fs_open_write(const char *tftp_name)
{
    fs_request_t req;
    fs_response_t rsp;

    memset(&req, 0, sizeof(req));
    req.cmd = FS_CMD_OPEN_WRITE;
    strncpy(req.path, tftp_name, sizeof(req.path) - 1U);

    if (!fs_rpc_call(&req, &rsp)) {
        return FR_TIMEOUT;
    }
    return rsp.fr;
}

FRESULT tftp_fs_read_block(uint8_t *buf, uint16_t buf_len, uint16_t *out_len)
{
    fs_request_t req;
    fs_response_t rsp;

    if (buf == NULL || out_len == NULL || buf_len == 0U) {
        return FR_INVALID_PARAMETER;
    }

    memset(&req, 0, sizeof(req));
    req.cmd = FS_CMD_READ;
    req.len = buf_len;

    if (!fs_rpc_call(&req, &rsp)) {
        g_tftp_dbg.err_step = 44U;   /* socket 侧 10s 内未收到 fs 响应 */
        g_tftp_dbg.last_fs_fr = (uint8_t)FR_TIMEOUT;
        return FR_TIMEOUT;
    }

    *out_len = rsp.len;
    if (rsp.fr == FR_OK && rsp.len > 0U) {
        memcpy(buf, rsp.data, rsp.len);
    }
    if (rsp.fr != FR_OK) {
        g_tftp_dbg.last_fs_fr = (uint8_t)rsp.fr;
        if (g_tftp_dbg.err_step == 0U) {
            g_tftp_dbg.err_step = 45U;
        }
    }
    return rsp.fr;
}

FRESULT tftp_fs_write_block(const uint8_t *buf, uint16_t len)
{
    fs_request_t req;
    fs_response_t rsp;

    if (buf == NULL) {
        return FR_INVALID_PARAMETER;
    }
    if (len == 0U) {
        return FR_OK;
    }

    memset(&req, 0, sizeof(req));
    req.cmd = FS_CMD_WRITE;
    req.len = len;
    memcpy(req.data, buf, len);

    if (!fs_rpc_call(&req, &rsp)) {
        g_tftp_dbg.last_fs_fr = (uint8_t)FR_TIMEOUT;
        return FR_TIMEOUT;
    }
    if (rsp.fr != FR_OK) {
        g_tftp_dbg.last_fs_fr = (uint8_t)rsp.fr;
    }
    return rsp.fr;
}

void tftp_fs_close(void)
{
    fs_request_t req;
    fs_response_t rsp;

    memset(&req, 0, sizeof(req));
    req.cmd = FS_CMD_CLOSE;
    (void)fs_rpc_call(&req, &rsp);
}
