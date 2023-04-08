#define LOG_MODULE "frame-mon"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "frame-mon.h"

#include "util/log.h"

#define FAST_FRAME_MARGIN_PERC 0.01
#define LATE_FRAME_MARGIN_PERC 0.01

static LARGE_INTEGER iidxhook_util_frame_monitor_perf_counter_freq;
static uint64_t iidxhook_util_frame_monitor_target_frame_time_us;

static uint64_t iidxhook_util_frame_monitor_total_frame_counter;
static int64_t iidxhook_util_frame_monitor_prev_frame_us;

static FILE* iidxhook_util_frame_monitor_file;

static int64_t iidxhook_util_frame_monitor_query_us()
{
    LARGE_INTEGER tick;

    QueryPerformanceCounter(&tick);

    return (tick.QuadPart * 1000000ll) / iidxhook_util_frame_monitor_perf_counter_freq.QuadPart;
}

static void iidxhook_util_frame_monitor_record_frame(int64_t frame_time_us)
{
    fprintf(iidxhook_util_frame_monitor_file, "%llu\t%llu\n",
        iidxhook_util_frame_monitor_total_frame_counter, frame_time_us);
    fflush(iidxhook_util_frame_monitor_file);
}

static void iidxhook_util_frame_monitor_report_fast_frames(int64_t frame_time_us)
{
    int64_t diff_us;
    double margin_perc;

    diff_us = (int64_t) iidxhook_util_frame_monitor_target_frame_time_us - frame_time_us;
    margin_perc = ((double) diff_us) / (double) iidxhook_util_frame_monitor_target_frame_time_us;

    if (diff_us > 0 && margin_perc > FAST_FRAME_MARGIN_PERC) {
        log_misc("Fast frame (%lld us > %lld us), diff %lld us (%lf %%)", 
            frame_time_us,
            iidxhook_util_frame_monitor_target_frame_time_us,
            diff_us,
            margin_perc * 100);
    }
}

static void iidxhook_util_frame_monitor_report_late_frames(int64_t frame_time_us)
{
    int64_t diff_us;
    double margin_perc;

    diff_us = frame_time_us - iidxhook_util_frame_monitor_target_frame_time_us;
    margin_perc = ((double) diff_us) / (double) iidxhook_util_frame_monitor_target_frame_time_us;

    if (diff_us > 0 && margin_perc > LATE_FRAME_MARGIN_PERC) {
        log_misc("Late frame (%lld us > %lld us), diff %lld us (%lf %%)", 
            frame_time_us,
            iidxhook_util_frame_monitor_target_frame_time_us,
            diff_us,
            margin_perc * 100);
    }
}

void iidxhook_util_frame_monitor_init(double target_frame_rate_hz)
{
    // Cache once
    QueryPerformanceFrequency(&iidxhook_util_frame_monitor_perf_counter_freq); 

    iidxhook_util_frame_monitor_target_frame_time_us = (1000.0 * 1000.0) / target_frame_rate_hz;

    iidxhook_util_frame_monitor_file = fopen("frame-mon.csv", "w+");
    fprintf(iidxhook_util_frame_monitor_file, "frame_num\tframe_time_us\n");

    log_info("Initialized, data output: frame-mon.csv");
}

void iidxhook_util_frame_monitor_update()
{
    int64_t now_us;
    int64_t frame_time_us;

    iidxhook_util_frame_monitor_total_frame_counter++;

    if (iidxhook_util_frame_monitor_prev_frame_us == 0) {
        iidxhook_util_frame_monitor_prev_frame_us = iidxhook_util_frame_monitor_query_us();
        return;
    }

    now_us = iidxhook_util_frame_monitor_query_us();
    frame_time_us = now_us - iidxhook_util_frame_monitor_prev_frame_us;
    
    // TODO have feature flags to enable/disable
    // - recording
    // - reporting
    //iidxhook_util_frame_monitor_record_frame(frame_time_us);
    iidxhook_util_frame_monitor_report_fast_frames(frame_time_us);
    iidxhook_util_frame_monitor_report_late_frames(frame_time_us);

    iidxhook_util_frame_monitor_prev_frame_us = now_us;
}