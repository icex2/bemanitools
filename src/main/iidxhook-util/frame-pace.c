#define LOG_MODULE "frame-pace"

#include <windows.h>

#include <stdbool.h>

#include "frame-pace.h"

#include "util/log.h"

static LARGE_INTEGER iidxhook_util_frame_pace_perf_counter_freq;
static uint64_t iidxhook_util_frame_pace_target_frame_time_us;

static int64_t iidxhook_util_frame_pace_prev_frame_time_us;

static int64_t iidxhook_util_frame_pace_query_us()
{
    LARGE_INTEGER tick;

    QueryPerformanceCounter(&tick);

    return (tick.QuadPart * 1000000ll) / iidxhook_util_frame_pace_perf_counter_freq.QuadPart;
}

static void iidxhook_util_frame_pace_sleep_us(int64_t sleep_us)
{
    if (sleep_us > 2000) {
        uint32_t ms = (sleep_us - 2000) / 1000;

        Sleep(ms);
    } else if (sleep_us > 0) {
        // Remark: This is only available on Vista and newer
        YieldProcessor();
    } else {
        // If behind time, don't delay any further
    }
}

void iidxhook_util_frame_pace_init(double target_frame_rate_hz)
{
    // Cache once
    QueryPerformanceFrequency(&iidxhook_util_frame_pace_perf_counter_freq);    

    iidxhook_util_frame_pace_target_frame_time_us = (uint64_t) (1000.0 * 1000.0 / target_frame_rate_hz);

    iidxhook_util_frame_pace_prev_frame_time_us = iidxhook_util_frame_pace_query_us();

    log_info("Initialized, target frame rate in hz %f, target frame time in us %llu", target_frame_rate_hz, iidxhook_util_frame_pace_target_frame_time_us);
}

void iidxhook_util_frame_pace_execute()
{
    int64_t now_us;
    int64_t diff_us;
    int64_t sleep_us;
    
    now_us = iidxhook_util_frame_pace_query_us();

    if (iidxhook_util_frame_pace_target_frame_time_us > 0) {
        while (true) {
            now_us = iidxhook_util_frame_pace_query_us();

            diff_us = now_us - iidxhook_util_frame_pace_prev_frame_time_us;

            if (diff_us >= iidxhook_util_frame_pace_target_frame_time_us) {
                break;
            }

            sleep_us = iidxhook_util_frame_pace_target_frame_time_us - diff_us;

            iidxhook_util_frame_pace_sleep_us(sleep_us);
        }
    }

    iidxhook_util_frame_pace_prev_frame_time_us = now_us;
}