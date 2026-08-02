/**
 * @file mon_cpu.c
 * @brief Idle 差分：non_idle% = (d_total - d_idle) / d_total
 */
#include "stm32f4xx.h"
#include "mon_cpu.h"

#include "FreeRTOS.h"
#include "task.h"

volatile MonCpu_Dbg_t g_mon_cpu;

static TaskStatus_t s_status[MON_CPU_TASK_MAX];

static uint32_t find_idle_run_time(const TaskStatus_t *px, UBaseType_t n)
{
    TaskHandle_t idle = xTaskGetIdleTaskHandle();
    UBaseType_t i;

    for (i = 0U; i < n; i++) {
        if (px[i].xHandle == idle) {
            return px[i].ulRunTimeCounter;
        }
    }
    return 0U;
}

static uint32_t delta_u32(uint32_t now, uint32_t prev)
{
    if (now >= prev) {
        return now - prev;
    }
    /* CYCCNT / 运行时计数器回绕 */
    return (UINT32_MAX - prev) + now + 1U;
}

void configureTimerForRunTimeStats(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t getRunTimeCounterValue(void)
{
    return DWT->CYCCNT;
}

uint32_t mon_cpu_sample(void)
{
    UBaseType_t n;
    uint32_t total;
    uint32_t idle;
    uint32_t d_total;
    uint32_t d_idle;
    uint32_t non_idle_pct;

    n = uxTaskGetSystemState(s_status, MON_CPU_TASK_MAX, &total);
    if (n == 0U) {
        g_mon_cpu.sample_skip++;
        return 0U;
    }

    idle = find_idle_run_time(s_status, n);

    if (g_mon_cpu.baseline_ready == 0U) {
        g_mon_cpu.last_total = total;
        g_mon_cpu.last_idle = idle;
        g_mon_cpu.baseline_ready = 1U;
        g_mon_cpu.sample_skip++;
        return 0U;
    }

    d_total = delta_u32(total, g_mon_cpu.last_total);
    if (d_total == 0U) {
        g_mon_cpu.sample_skip++;
        return 0U;
    }

    d_idle = delta_u32(idle, g_mon_cpu.last_idle);
    if (d_idle > d_total) {
        /* 统计异常，重建基线 */
        g_mon_cpu.last_total = total;
        g_mon_cpu.last_idle = idle;
        g_mon_cpu.sample_skip++;
        return 0U;
    }

    non_idle_pct = ((d_total - d_idle) * 100U + d_total - 1U) / d_total;

    g_mon_cpu.last_window_non_idle_pct = non_idle_pct;
    g_mon_cpu.last_total = total;
    g_mon_cpu.last_idle = idle;
    g_mon_cpu.sample_ok++;

    if (non_idle_pct > g_mon_cpu.peak_non_idle_pct) {
        g_mon_cpu.peak_non_idle_pct = non_idle_pct;
    }

    return non_idle_pct;
}

void mon_cpu_reset_peak(void)
{
    g_mon_cpu.peak_non_idle_pct = 0U;
    g_mon_cpu.last_window_non_idle_pct = 0U;
}

uint32_t mon_cpu_peak_non_idle_pct(void)
{
    return g_mon_cpu.peak_non_idle_pct;
}

uint32_t mon_cpu_last_window_non_idle_pct(void)
{
    return g_mon_cpu.last_window_non_idle_pct;
}

uint8_t mon_cpu_peak_ok(uint32_t limit_pct)
{
    if (g_mon_cpu.peak_non_idle_pct > limit_pct) {
        return 0U;
    }
    return 1U;
}
