#define LOG_MODULE "iidxhook-d3d9-frame-mon"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "frame-mon.h"

#include "util/datetime.h"
#include "util/log.h"
#include "util/str.h"

#define FAST_FRAME_MARGIN_PERC 0.01
#define LATE_FRAME_MARGIN_PERC 0.01

static bool iidxhook_d3d9_frame_pace_initialized;

static LARGE_INTEGER iidxhook_d3d9_frame_mon_perf_counter_freq;
static uint64_t iidxhook_d3d9_frame_mon_target_frame_time_us;

static uint64_t iidxhook_d3d9_frame_mon_total_frame_counter;
static int64_t iidxhook_d3d9_frame_mon_prev_frame_us;

static float iidxhook_d3d9_frame_mon_log_slow_frames_margin;
static float iidxhook_d3d9_frame_mon_log_fast_frames_margin;
static FILE* iidxhook_d3d9_frame_mon_stats_file;

static int64_t iidxhook_d3d9_frame_mon_time_query_us()
{
    LARGE_INTEGER tick;

    QueryPerformanceCounter(&tick);

    return (tick.QuadPart * 1000000ll) / iidxhook_d3d9_frame_mon_perf_counter_freq.QuadPart;
}

static void iidxhook_d3d9_frame_mon_record_frame(int64_t frame_time_us)
{
    fprintf(iidxhook_d3d9_frame_mon_stats_file, "%llu\t%llu\n",
        iidxhook_d3d9_frame_mon_total_frame_counter, frame_time_us);
    fflush(iidxhook_d3d9_frame_mon_stats_file);
}

static void iidxhook_d3d9_frame_mon_log_fast_frames(int64_t frame_time_us)
{
    int64_t diff_us;
    double margin_perc;

    diff_us = (int64_t) iidxhook_d3d9_frame_mon_target_frame_time_us - frame_time_us;
    margin_perc = ((double) diff_us) / (double) iidxhook_d3d9_frame_mon_target_frame_time_us;

    if (diff_us > 0 && margin_perc > iidxhook_d3d9_frame_mon_log_fast_frames_margin) {
        log_misc("FAST frame (%llu), %lld us > %lld us, diff %lld us (%lf %%)", 
            iidxhook_d3d9_frame_mon_total_frame_counter - 1,
            frame_time_us,
            iidxhook_d3d9_frame_mon_target_frame_time_us,
            diff_us,
            margin_perc * 100);
    }
}

static void iidxhook_d3d9_frame_mon_log_slow_frames(int64_t frame_time_us)
{
    int64_t diff_us;
    double margin_perc;

    diff_us = frame_time_us - iidxhook_d3d9_frame_mon_target_frame_time_us;
    margin_perc = ((double) diff_us) / (double) iidxhook_d3d9_frame_mon_target_frame_time_us;

    if (diff_us > 0 && margin_perc > iidxhook_d3d9_frame_mon_log_slow_frames_margin) {
        log_misc("SLOW frame (%llu), %lld us > %lld us, diff %lld us (%lf %%)", 
            iidxhook_d3d9_frame_mon_total_frame_counter - 1,
            frame_time_us,
            iidxhook_d3d9_frame_mon_target_frame_time_us,
            diff_us,
            margin_perc * 100);
    }
}

static void iidxhook_d3d9_frame_mon_do_post_frame()
{
    int64_t now_us;
    int64_t frame_time_us;

    iidxhook_d3d9_frame_mon_total_frame_counter++;

    now_us = iidxhook_d3d9_frame_mon_time_query_us();
    frame_time_us = now_us - iidxhook_d3d9_frame_mon_prev_frame_us;
    
    if (iidxhook_d3d9_frame_mon_stats_file) {
        iidxhook_d3d9_frame_mon_record_frame(frame_time_us);
    }

    if (iidxhook_d3d9_frame_mon_log_slow_frames_margin > 0.0) {
        iidxhook_d3d9_frame_mon_log_slow_frames(frame_time_us);
    }
    
    if (iidxhook_d3d9_frame_mon_log_fast_frames_margin > 0.0) {
        iidxhook_d3d9_frame_mon_log_fast_frames(frame_time_us);
    }

    iidxhook_d3d9_frame_mon_prev_frame_us = now_us;
}

static FILE* iidxhook_d3d9_frame_mon_open_out_file(const char* out_path)
{
    char module_name[MAX_PATH];
    char now_timestamp[64];
    char out_file_path[MAX_PATH];
    FILE* file;
    const char* filename_module;

    GetModuleFileName(NULL, module_name, sizeof(module_name));

    filename_module = strrchr(module_name, '\\');

    if (filename_module == NULL) {
        // no backslashes found, use the entire path
        filename_module = module_name;
    } else {
        // skip the backslash
        filename_module++;
    }

    if (!util_datetime_now_formated(now_timestamp, sizeof(now_timestamp))) {
        log_warning("Formatting timestamp failed");
    }

    memset(out_file_path, 0, sizeof(out_file_path));

    if (!str_multi_cat(out_file_path, sizeof(out_file_path), out_path, "\\", 
            "frame-mon-stats-", filename_module, "-", now_timestamp, ".csv")) {
        log_fatal("Formatting output file name failed");
    }

    file = fopen(out_file_path, "w+");

    if (!file) {
        log_fatal("Failed opening frame stats out file %s, errno: %d", out_file_path, errno);
    }

    log_info("Frame stats output file: %s", out_file_path);

    return file;
}

static void iidxhook_d3d9_frame_mon_stats_file_print_header()
{
    fprintf(iidxhook_d3d9_frame_mon_stats_file, "frame_num\tframe_time_us\n");
    fflush(iidxhook_d3d9_frame_mon_stats_file);
}

void iidxhook_d3d9_frame_mon_init(
        double target_frame_rate_hz,
        float log_slow_frames_margin,
        float log_fast_frames_margin,
        const char* frame_stats_out_path)
{
    iidxhook_d3d9_frame_mon_log_slow_frames_margin = log_slow_frames_margin;
    iidxhook_d3d9_frame_mon_log_fast_frames_margin = log_fast_frames_margin;

    if (iidxhook_d3d9_frame_mon_log_slow_frames_margin > 0.0) {
        log_info("Logging SLOW frames enabled, margin: %f", iidxhook_d3d9_frame_mon_log_slow_frames_margin);
    }

    if (iidxhook_d3d9_frame_mon_log_fast_frames_margin) {
        log_info("Logging FAST frames enabled, margin: %f", iidxhook_d3d9_frame_mon_log_fast_frames_margin);
    }

    if (frame_stats_out_path) {
        iidxhook_d3d9_frame_mon_stats_file = iidxhook_d3d9_frame_mon_open_out_file(frame_stats_out_path);
        iidxhook_d3d9_frame_mon_stats_file_print_header();
    }

    // Cache once
    QueryPerformanceFrequency(&iidxhook_d3d9_frame_mon_perf_counter_freq); 
    iidxhook_d3d9_frame_mon_target_frame_time_us = (1000.0 * 1000.0) / target_frame_rate_hz;
    // Init to avoid buggy behavior after first frame
    iidxhook_d3d9_frame_mon_prev_frame_us = iidxhook_d3d9_frame_mon_time_query_us();

    iidxhook_d3d9_frame_pace_initialized = true;

    log_info("Initialized");
}

HRESULT iidxhook_d3d9_frame_mon_d3d9_irp_handler(struct hook_d3d9_irp *irp)
{
    HRESULT hr;

    log_assert(irp);

    if (!iidxhook_d3d9_frame_pace_initialized) {
        return hook_d3d9_irp_invoke_next(irp);
    }

    if (irp->op == HOOK_D3D9_IRP_OP_DEV_PRESENT) {
        hr = hook_d3d9_irp_invoke_next(irp);

        if (hr == S_OK) {
            iidxhook_d3d9_frame_mon_do_post_frame();
        }

        return hr;
    } else {
        return hook_d3d9_irp_invoke_next(irp);
    }
}