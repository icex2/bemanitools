#ifndef IIDXHOOK_UTIL_PROC_MCORE_H
#define IIDXHOOK_UTIL_PROC_MCORE_H

#include <windows.h>

#include <stdbool.h>

enum IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES {
    IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_INVALID = 0,
    IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_AUTO = 1,
    IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_2 = 2,
    IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_4 = 3,
};

void iidxhook_util_proc_mcore_init(
        int main_thread_id,
        enum IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES cpu_cores);

#endif