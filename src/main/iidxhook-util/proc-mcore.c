#define LOG_MODULE "iidxhook-util-proc-mcore"

#include "hook/table.h"

#include "util/defs.h"
#include "util/log.h"
#include "util/mem.h"
#include "util/proc.h"

#include "proc-mcore.h"

static HANDLE STDCALL my_CreateThread(
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    SIZE_T dwStackSize,
    LPTHREAD_START_ROUTINE lpStartAddress,
    __drv_aliasesMem LPVOID lpParameter,
    DWORD dwCreationFlags,
    LPDWORD lpThreadId);
static BOOL STDCALL my_SetThreadPriority(
    HANDLE hThread,
    int nPriority);

static HANDLE (STDCALL *real_CreateThread)(
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    SIZE_T dwStackSize,
    LPTHREAD_START_ROUTINE lpStartAddress,
    __drv_aliasesMem LPVOID lpParameter,
    DWORD dwCreationFlags,
    LPDWORD lpThreadId);
static BOOL (STDCALL *real_SetThreadPriority)(
    HANDLE hThread,
    int nPriority);

static void iidxhook_util_proc_mcore_patch_thread(int thread_id, int priority, uint32_t affinity, const char* ident);

static const struct hook_symbol iidxhok_util_proc_mcore_hook_syms[] = {
    {
        .name = "CreateThread",
        .patch = my_CreateThread,
        .link = (void **) &real_CreateThread,
    },
    {
        .name = "SetThreadPriority",
        .patch = my_SetThreadPriority,
        .link = (void **) &real_SetThreadPriority,
    },
};

static enum IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES iidxhook_util_proc_mcore_cpu_cores;
static int iidxhook_util_proc_mcore_thread_priority_blocklist[3];

static HANDLE STDCALL my_CreateThread(
        LPSECURITY_ATTRIBUTES lpThreadAttributes,
        SIZE_T dwStackSize,
        LPTHREAD_START_ROUTINE lpStartAddress,
        __drv_aliasesMem LPVOID lpParameter,
        DWORD dwCreationFlags,
        LPDWORD lpThreadId)
{
    HANDLE handle;
    int thread_id;
    int priority;
    uint32_t affinity;

    handle = real_CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);   

    // TODO for 11+, this uses beginthread and beginthreadx wrappers which pass custom structures here

    // TODO for 11+, we have to trap creation of the IO threads here?

    // M:proc-mon: CreateThread(module_handle 00400000, module_name Y:\iidx\11\JAA\bm2dx.exe, 
    // current_thread_id 14936, lpThreadAttributes 00000000, dwStackSize 0, lpStartAddress 004A4E73, 
    // lpParameter 0B472860, dwCreationFlags 0x4, lpThreadId 0B472860)

    // CreateThread(module_handle 00400000, module_name Y:\iidx\11\JAA\bm2dx.exe, 
    // current_thread_id 14936, lpThreadAttributes 00000000, dwStackSize 0, lpStartAddress 004A4E73, 
    // lpParameter 0B4726C0, dwCreationFlags 0x4, lpThreadId 0B4726C0)

    // Ensure that any new threads getting created are scheduled to the right cores with normal
    // priorities
    if (handle != INVALID_HANDLE_VALUE) {
        thread_id = GetThreadId(handle);
        priority = THREAD_PRIORITY_NORMAL;
        affinity = PROC_THREAD_CPU_AFFINITY_CORE(2) | PROC_THREAD_CPU_AFFINITY_CORE(3);

        iidxhook_util_proc_mcore_patch_thread(thread_id, priority, affinity, "other");
    }

    return handle;
}

static BOOL STDCALL my_SetThreadPriority(
        HANDLE hThread,
        int nPriority)
{
    int thread_id;

    thread_id = GetThreadId(hThread);

    for (int i = 0; i < sizeof(iidxhook_util_proc_mcore_thread_priority_blocklist); i++) {
        if (thread_id == iidxhook_util_proc_mcore_thread_priority_blocklist[i]) {
            log_misc("Blocked priority %d on thread id %d", nPriority, thread_id);
            return TRUE;
        }     
    }

    if (nPriority != THREAD_PRIORITY_NORMAL) {
        log_misc("Enforcing priority %d -> %d on thread id %d", nPriority, THREAD_PRIORITY_NORMAL, thread_id);
    }

    // Enforce normal priority on all threads
    return real_SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
}

static void iidxhook_util_proc_mcore_scan_and_log_threads()
{
    size_t count;
    struct proc_thread_info* thread_infos;
    char origin_module_name[MAX_PATH];

    count = proc_thread_scan_threads_current_process(&thread_infos);

    for (size_t i = 0; i < count; i++) {
        if (!proc_thread_proc_get_origin_module_name(
                thread_infos[i].proc,
                origin_module_name,
                sizeof(origin_module_name))) {
            log_fatal("Getting origin module name of thread proc %p failed", thread_infos[i].proc);
        }

        log_misc("thread id %d, proc %p, priority %d, origin_module %p, origina_module_name %s",
            thread_infos[i].id,
            thread_infos[i].proc,
            thread_infos[i].priority,
            thread_infos[i].origin_module,
            origin_module_name);
    }
}

static size_t iidxhook_util_proc_mcore_find_ezusb_io_threads(struct proc_thread_info **io_thread_infos)
{
    size_t count;
    struct proc_thread_info* thread_infos;
    char origin_module_name[MAX_PATH];
    size_t count_ezusb;

    log_assert(io_thread_infos);

    count = proc_thread_scan_threads_current_process(&thread_infos);

    *io_thread_infos = NULL;
    count_ezusb = 0;

    for (size_t i = 0; i < count; i++) {
        if (!proc_thread_proc_get_origin_module_name(
                thread_infos[i].proc,
                origin_module_name,
                sizeof(origin_module_name))) {
            log_fatal("Getting origin module name of thread proc %p failed", thread_infos[i].proc);
        }

        if (!strcmp(origin_module_name, "ezusb.dll")) {
            *io_thread_infos = xrealloc(*io_thread_infos, (count_ezusb + 1) * sizeof(struct proc_thread_info));

            memcpy(&(*io_thread_infos)[count_ezusb], &thread_infos[i], sizeof(struct proc_thread_info));

            count_ezusb++;
        }
    }

    return count_ezusb;
}

static void iidxhook_util_proc_mcore_patch_thread(int thread_id, int priority, uint32_t affinity, const char* ident)
{
    if (!proc_thread_set_priority(thread_id, priority)) {
        log_fatal("Setting thread(%s) priority, id %d, priority %d failed", ident, thread_id, priority);
    }

    if (!proc_thread_set_affinity(thread_id, affinity)) {
        log_fatal("Setting thread(%s) affinity, id %d, affinity %d failed", ident, thread_id, affinity);
    }

    log_misc("Patch(%s) thread id %d, priority %d, cpu affinity 0x%x", ident, thread_id, priority, affinity);
}

static void iidxhook_util_proc_mcore_patch_io_threads()
{
    size_t count;
    struct proc_thread_info* thread_infos;
    char origin_module_name[MAX_PATH];
    int priority;
    uint32_t affinity;

    count = iidxhook_util_proc_mcore_find_ezusb_io_threads(&thread_infos);

    switch (iidxhook_util_proc_mcore_cpu_cores) {
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_2:
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_4:
            priority = THREAD_PRIORITY_TIME_CRITICAL;
            affinity = PROC_THREAD_CPU_AFFINITY_CORE(1);
            break;

        default:
            log_fatal("Illegal state, case %d", iidxhook_util_proc_mcore_cpu_cores);
            break;
    }
    
    // TODO how to handle this if we don't have the ezusb module? how to detect which thread is the IO thread?

    log_assert(count == 2);

    for (size_t i = 0; i < count; i++) {
        if (!proc_thread_proc_get_origin_module_name(
                thread_infos[i].proc,
                origin_module_name,
                sizeof(origin_module_name))) {
            log_fatal("Getting origin module name of thread proc %p failed", thread_infos[i].proc);
        }

        log_info("EZUSB IO: id %d, proc %p, priority %d, origin_module %p, origin_module_name %s",
            thread_infos[i].id,
            thread_infos[i].proc,
            thread_infos[i].priority,
            thread_infos[i].origin_module,
            origin_module_name);

        iidxhook_util_proc_mcore_patch_thread(thread_infos[i].id, priority, affinity, "ezusb");

        iidxhook_util_proc_mcore_thread_priority_blocklist[i] = thread_infos[i].id;
    }
}

static void iidxhook_util_proc_mcore_patch_main_thread(int thread_id)
{
    int priority;
    uint32_t affinity;

    switch (iidxhook_util_proc_mcore_cpu_cores) {
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_2:
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_4:
            priority = THREAD_PRIORITY_TIME_CRITICAL;
            affinity = PROC_THREAD_CPU_AFFINITY_CORE(0);
            break;

        default:
            log_fatal("Illegal state, case %d", iidxhook_util_proc_mcore_cpu_cores);
            break;
    }

    iidxhook_util_proc_mcore_patch_thread(thread_id, priority, affinity, "main");

    iidxhook_util_proc_mcore_thread_priority_blocklist[2] = thread_id;
}

static void iidxhook_util_proc_mcore_patch_other_threads(int main_thread_id)
{
    size_t count;
    struct proc_thread_info* thread_infos;
    char origin_module_name[MAX_PATH];
    int priority;
    uint32_t affinity;

    count = proc_thread_scan_threads_current_process(&thread_infos);

    switch (iidxhook_util_proc_mcore_cpu_cores) {
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_2:
            priority = THREAD_PRIORITY_NORMAL;
            affinity = PROC_THREAD_CPU_AFFINITY_CORE(1);
            break;

        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_4:
            priority = THREAD_PRIORITY_NORMAL;
            affinity = PROC_THREAD_CPU_AFFINITY_CORE(2) | PROC_THREAD_CPU_AFFINITY_CORE(3);
            break;

        default:
            log_fatal("Illegal state, case %d", iidxhook_util_proc_mcore_cpu_cores);
            break;
    }

    for (size_t i = 0; i < count; i++) {
        if (!proc_thread_proc_get_origin_module_name(
                thread_infos[i].proc,
                origin_module_name,
                sizeof(origin_module_name))) {
            log_fatal("Getting origin module name of thread proc %p failed", thread_infos[i].proc);
        }

        // skip
        if (!strcmp(origin_module_name, "ezusb.dll")) {
            continue;
        }

        // skip
        if (thread_infos[i].id == main_thread_id) {
            continue;
        }

        iidxhook_util_proc_mcore_patch_thread(thread_infos[i].id, priority, affinity, "other");
    }
}

static enum IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES iidxhook_util_proc_mcore_auto_detected()
{
    SYSTEM_INFO sysInfo;
    enum IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES res;
    
    GetSystemInfo(&sysInfo);

    if (sysInfo.dwNumberOfProcessors == 1) {
        res = IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_1;
    } else if (sysInfo.dwNumberOfProcessors >= 2 && sysInfo.dwNumberOfProcessors < 4) {
        res = IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_2;
    } else if (sysInfo.dwNumberOfProcessors > 4) {
        res = IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_4;
    } else {
        log_fatal("Illegal state determining CPU core count, value %ld", sysInfo.dwNumberOfProcessors);
        res = IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_1;
    }

    log_info("Auto detected CPU count %ld, applying mode %d", sysInfo.dwNumberOfProcessors, res);

    return res;
}

void iidxhook_util_proc_mcore_init(
        int main_thread_id,
        enum IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES cpu_cores)
{
    switch (cpu_cores) {
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_AUTO:
            iidxhook_util_proc_mcore_cpu_cores = iidxhook_util_proc_mcore_auto_detected();
            break;

        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_1:
            // Do nothing in this case and keep everything stock
            log_info("Single core option does not apply changes, skip module");
            return;

        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_2:
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_4:
            iidxhook_util_proc_mcore_cpu_cores = cpu_cores;
            break;

        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_INVALID:
        default:
            log_fatal("Illegal state, case %d", cpu_cores);
            break;
    }

    // TODO trap and schedule sound thread to own core on 4 core machine?
    // core 0: main/gfx
    // core 1: IO
    // core 2: sound
    // core 4: other
    // ???

    // TODO don't fix thread prios on 11+?

    iidxhook_util_proc_mcore_scan_and_log_threads();
    iidxhook_util_proc_mcore_patch_main_thread(main_thread_id);
    // iidxhook_util_proc_mcore_patch_io_threads();
    // iidxhook_util_proc_mcore_patch_other_threads(main_thread_id);

    hook_table_apply(
            NULL, "kernel32.dll", iidxhok_util_proc_mcore_hook_syms, lengthof(iidxhok_util_proc_mcore_hook_syms));

    log_info("Initialized");
}