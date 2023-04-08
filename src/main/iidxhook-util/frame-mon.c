#define LOG_MODULE "frame-mon"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "frame-mon.h"

#include "util/log.h"

static LARGE_INTEGER iidxhook_util_frame_monitor_perf_counter_freq;

static uint64_t iidxhook_util_frame_monitor_total_frame_counter;
static int64_t iidxhook_util_frame_monitor_prev_frame_us;

static FILE* iidxhook_util_frame_monitor_file;

static int64_t iidxhook_util_frame_monitor_query_us()
{
    LARGE_INTEGER tick;

    QueryPerformanceCounter(&tick);

    return (tick.QuadPart * 1000000ll) / iidxhook_util_frame_monitor_perf_counter_freq.QuadPart;
}

void iidxhook_util_frame_monitor_init()
{
    // Cache once
    QueryPerformanceFrequency(&iidxhook_util_frame_monitor_perf_counter_freq); 

    iidxhook_util_frame_monitor_file = fopen("frame-mon.csv", "w+");
    fprintf(iidxhook_util_frame_monitor_file, "frame_num\tframe_time_us\n");

    log_info("Initialized, data output: frame-mon.csv");
}

void iidxhook_util_frame_monitor_record_frame()
{
    int64_t now_us;
    int64_t diff_us;

    iidxhook_util_frame_monitor_total_frame_counter++;

    if (iidxhook_util_frame_monitor_prev_frame_us == 0) {
        iidxhook_util_frame_monitor_prev_frame_us = iidxhook_util_frame_monitor_query_us();
        return;
    }

    now_us = iidxhook_util_frame_monitor_query_us();
    diff_us = now_us - iidxhook_util_frame_monitor_prev_frame_us;
    
    fprintf(iidxhook_util_frame_monitor_file, "%llu\t%llu\n",
        iidxhook_util_frame_monitor_total_frame_counter, diff_us);
    fflush(iidxhook_util_frame_monitor_file);

    iidxhook_util_frame_monitor_prev_frame_us = now_us;
}