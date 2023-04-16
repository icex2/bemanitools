#define LOG_MODULE "iidxhook-d3d-frame-pace"

#include <windows.h>

#include <stdbool.h>

#include "hook/table.h"

#include "util/log.h"

#include "frame-pace.h"

static void STDCALL my_Sleep(DWORD dwMilliseconds);
static void (STDCALL *real_Sleep)(DWORD dwMilliseconds);

static DWORD STDCALL my_SleepEx(DWORD dwMilliseconds, BOOL bAlertable);
static DWORD (STDCALL *real_SleepEx)(DWORD dwMilliseconds, BOOL bAlertable);

static const struct hook_symbol iidxhok_d3d9_frame_pace_hook_syms[] = {
    {
        .name = "Sleep",
        .patch = my_Sleep,
        .link = (void **) &real_Sleep,
    },
    {
        .name = "SleepEx",
        .patch = my_SleepEx,
        .link = (void **) &real_SleepEx,
    },
};

static bool iidxhook_d3d9_frame_pace_initialized;

static DWORD iidxhook_d3d9_frame_pace_main_thread_id = -1;
static LARGE_INTEGER iidxhook_d3d9_frame_pace_perf_counter_freq;
static uint64_t iidxhook_d3d9_frame_pace_target_frame_time_us;

static int64_t iidxhook_d3d9_frame_pace_prev_frame_time_us;

static int64_t iidxhook_d3d9_frame_pace_query_us()
{
    LARGE_INTEGER tick;

    QueryPerformanceCounter(&tick);

    return (tick.QuadPart * 1000000ll) / iidxhook_d3d9_frame_pace_perf_counter_freq.QuadPart;
}

static void iidxhook_d3d9_frame_pace_sleep_us(int64_t sleep_us)
{
    if (sleep_us > 2000) {
        uint32_t ms = (sleep_us - 2000) / 1000;

        Sleep(ms);
    } else if (sleep_us > 0) {
        // Remark: This is only available on Vista and newer
        // TODO make this a feature switch to enable/disable burning CPU?
        // YieldProcessor();
        
    } else {
        // If behind time, don't delay any further
    }
}

// Source and reference implementation:
// https://nkga.github.io/post/frame-pacing-analysis-of-the-game-loop/
static void iidxhook_d3d9_frame_pace_do_post_frame()
{
    int64_t now_us;
    int64_t diff_us;
    int64_t sleep_us;
    
    now_us = iidxhook_d3d9_frame_pace_query_us();

    if (iidxhook_d3d9_frame_pace_target_frame_time_us > 0) {
        while (true) {
            now_us = iidxhook_d3d9_frame_pace_query_us();

            diff_us = now_us - iidxhook_d3d9_frame_pace_prev_frame_time_us;

            if (diff_us >= iidxhook_d3d9_frame_pace_target_frame_time_us) {
                break;
            }

            sleep_us = iidxhook_d3d9_frame_pace_target_frame_time_us - diff_us;

            iidxhook_d3d9_frame_pace_sleep_us(sleep_us);
        }
    }

    iidxhook_d3d9_frame_pace_prev_frame_time_us = now_us;
}

// TODO must be renamed to framerate monitor with smoother/pacer
// TODO have feature flag to print framerate performance counters etc every X seconds
// as misc debug log output
// TODO make sure to record a decent amount of data/frame time accordingly over these
// seconds to report proper avg. frame time/rate, min, max, p95, p99, p999
// TODO move this to a separate module that can be re-used on d3d9ex

// fill up unused frametime on short frames to simulate hardware accuracy
// and match the timing of the target monitor's refresh rate as close as possible
// this fixes frame pacing issues with too short frames not being smoothened
// correctly by the game which either relies entirely on the hardware/GPU driver
// to do that or on tricoro+ era games, on SleepEx which only has max of 1 ms
// accuracy. the further the target monitor refresh rate is away from the desired
// refresh rate, e.g. 60 hz vsync, the more apparent the frame pacing issues
// become in the form of "random stuttering during gameplay"

static void STDCALL my_Sleep(DWORD dwMilliseconds)
{
    // Heuristic, but seems to kill the poorly implemented frame pacing code
    // fairly reliable without impacting other parts of the code negatively
    if (iidxhook_d3d9_frame_pace_main_thread_id == GetCurrentThreadId()) {
        if (dwMilliseconds <= 16) {
            return;
        }
    }

    real_Sleep(dwMilliseconds);
}

static DWORD STDCALL my_SleepEx(DWORD dwMilliseconds, BOOL bAlertable)
{
    // Heuristic, but applies only in two spots
    // - frame pacing code (dynamic value)
    // - Another spot with sleep time set to 1 -> reduces CPU banging
    if (iidxhook_d3d9_frame_pace_main_thread_id == GetCurrentThreadId()) {
        if (dwMilliseconds <= 16) {
            return 0;
        }
    }

    return real_SleepEx(dwMilliseconds, bAlertable);
}

void iidxhook_d3d9_frame_pace_init(DWORD main_thread_id, double target_frame_rate_hz)
{
    log_assert(main_thread_id != -1);

    // Cache once
    QueryPerformanceFrequency(&iidxhook_d3d9_frame_pace_perf_counter_freq);    

    iidxhook_d3d9_frame_pace_main_thread_id = main_thread_id;
    iidxhook_d3d9_frame_pace_target_frame_time_us = (uint64_t) (1000.0 * 1000.0 / target_frame_rate_hz);
    iidxhook_d3d9_frame_pace_prev_frame_time_us = iidxhook_d3d9_frame_pace_query_us();

    iidxhook_d3d9_frame_pace_initialized = true;

    hook_table_apply(
            NULL, "kernel32.dll", iidxhok_d3d9_frame_pace_hook_syms, lengthof(iidxhok_d3d9_frame_pace_hook_syms));

    log_info("Initialized, target frame rate in hz %f, target frame time in us %llu", target_frame_rate_hz, iidxhook_d3d9_frame_pace_target_frame_time_us);
}

HRESULT iidxhook_d3d9_frame_pace_d3d9_irp_handler(struct hook_d3d9_irp *irp)
{
    HRESULT hr;

    log_assert(irp);

    if (!iidxhook_d3d9_frame_pace_initialized) {
        return hook_d3d9_irp_invoke_next(irp);
    }

    if (irp->op == HOOK_D3D9_IRP_OP_DEV_PRESENT) {
        hr = hook_d3d9_irp_invoke_next(irp);

        if (hr == S_OK) {
            iidxhook_d3d9_frame_pace_do_post_frame();
        }

        return hr;
    } else {
        return hook_d3d9_irp_invoke_next(irp);
    }
}