/**
 * @file mon_cpu.h
 * @brief 基于 Idle 任务 ulRunTimeCounter 差分的非空闲 CPU 窗口/峰值统计。
 *
 * 用法：
 *   1. configGENERATE_RUN_TIME_STATS=1（FreeRTOSConfig.h 已配）
 *   2. 调度器运行后，每 MON_CPU_SAMPLE_MS 调用 mon_cpu_sample()
 *   3. mon_cpu_peak_non_idle_pct() 读峰值；mon_cpu_peak_ok(70) 做 70% 门禁
 */
#ifndef MON_CPU_H
#define MON_CPU_H

#include <stdint.h>

#ifndef MON_CPU_TASK_MAX
#define MON_CPU_TASK_MAX  20U
#endif

#ifndef MON_CPU_SAMPLE_MS
#define MON_CPU_SAMPLE_MS 1000U
#endif

/** 供调试器观察 */
typedef struct {
    volatile uint32_t last_total;
    volatile uint32_t last_idle;
    volatile uint32_t last_window_non_idle_pct;
    volatile uint32_t peak_non_idle_pct;
    volatile uint32_t sample_ok;
    volatile uint32_t sample_skip;
    volatile uint8_t  baseline_ready;
} MonCpu_Dbg_t;

extern volatile MonCpu_Dbg_t g_mon_cpu;

void configureTimerForRunTimeStats(void);
uint32_t getRunTimeCounterValue(void);

/**
 * @return 本窗口非空闲 CPU 占比 0~100；首帧或无效窗口返回 0
 */
uint32_t mon_cpu_sample(void);

void mon_cpu_reset_peak(void);

uint32_t mon_cpu_peak_non_idle_pct(void);
uint32_t mon_cpu_last_window_non_idle_pct(void);

/** limit_pct 例如 70：峰值非空闲 <= 70% 则返回 1 */
uint8_t mon_cpu_peak_ok(uint32_t limit_pct);

#endif /* MON_CPU_H */
