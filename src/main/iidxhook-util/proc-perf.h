#ifndef IIDXHOOK_UTIL_PROC_PERF_H
#define IIDXHOOK_UTIL_PROC_PERF_H

#include <windows.h>

#include <stdbool.h>

void iidxhook_util_proc_perf_init(
        HANDLE main_thread_handle,
        bool main_thread_force_realtime_priority,
        bool multi_core_patch);

#endif