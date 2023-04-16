#define LOG_MODULE "iidxhook-util-proc-perf"

#include "hook/table.h"

#include "util/defs.h"
#include "util/log.h"
#include "util/mem.h"
#include "util/proc.h"

#include "proc-mcore.h"

static BOOL STDCALL my_SetThreadPriority(
    HANDLE hThread,
    int nPriority);
static BOOL (STDCALL *real_SetThreadPriority)(
    HANDLE hThread,
    int nPriority);

static const struct hook_symbol iidxhok_util_proc_mcore_hook_syms[] = {
    {
        .name = "SetThreadPriority",
        .patch = my_SetThreadPriority,
        .link = (void **) &real_SetThreadPriority,
    },
};

static bool iidxhook_util_proc_perf_main_thread_force_realtime_priority;

static HANDLE iidxhook_util_proc_perf_main_thread_handle;

// static const struct hook_symbol ezusb_hook_syms[] = {
//     {
//         .name = "SetThreadPriority",
//         .patch = my_SetThreadPriority,
//         .link = (void **) &real_SetThreadPriority,
//     },
// };

static BOOL STDCALL my_SetThreadPriority(
        HANDLE hThread,
        int nPriority)
{
    if (hThread == iidxhook_util_proc_perf_main_thread_handle) {
        log_misc("Force keep main thread on realtime priority");
        return real_SetThreadPriority(hThread, REALTIME_PRIORITY_CLASS);
    } else {
        // TODO ???
        //log_info("Patch thread priority of thread %p, priority %d -> 0 (normal)", hThread, nPriority);
    }   

    // TODO what about patching the ezusb IO threads on 9 and 10 with separate ezusb lib?
   
    return real_SetThreadPriority(hThread, nPriority);
}

static uint8_t iidxhook_util_proc_mcore_get_hw_cpu_count()
{
    SYSTEM_INFO sysInfo;
    
    GetSystemInfo(&sysInfo);

    return sysInfo.dwNumberOfProcessors;
}

static size_t iidxhook_util_proc_mcore_scan_for_ezusb_io_threads(struct proc_thread_info **io_thread_infos)
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

static void patch_ezusb_threads()
{
    size_t count;
    struct proc_thread_info* thread_infos;
    char origin_module_name[MAX_PATH];

    count = iidxhook_util_proc_mcore_scan_for_ezusb_io_threads(&thread_infos);

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
    }
}

void iidxhook_util_proc_mcore_init(
        int main_thread_id,
        enum IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES cpu_cores)
{
    uint8_t cpu_core_count;
    HANDLE main_thread;

    main_thread = proc_thread_get_handle(main_thread_id);

    patch_ezusb_threads();

    switch (cpu_cores) {
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_AUTO:
            iidxhook_util_proc_mcore_get_hw_cpu_count();

            // TODO check return value and either go for 2 or 4 core optimization

            break;

        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_2:
            cpu_core_count = 2;
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_4:
            cpu_core_count = 4;
            break; 
        case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_INVALID:
        default:
            log_fatal("Illegal state, value: %d", cpu_cores);
            break;
    }

    // TODO tricky, need a way to trap all thread creation, see proc-mon
    // and a heuristic to determine which thread is which
    // probably good to have a separate module with an IRP style thing like
    // the d3d9 hook module that allows to patch into thread creation, starting etc
    // -> hooklib/proc?

    // cpu 1:
        // don't do anything because single core
    // cpu 2:
        // boost main thread prio and pin it to single core, pin remaining threads to second core
    // cpu 4:
        // boost main thread prio and pin to core 1
        // boost ezusb/IO thread prio and pin to core 2
        // pin remaining threads to cores 3 and 4

    iidxhook_util_proc_perf_main_thread_handle = main_thread;
    iidxhook_util_proc_perf_main_thread_force_realtime_priority = true;

    if (iidxhook_util_proc_perf_main_thread_force_realtime_priority) {
        // Boost main thread prio to max to ensure the render loop is prioritzed on
        // thread scheduling which reduces microstutters
        SetThreadPriority(main_thread, REALTIME_PRIORITY_CLASS);

        hook_table_apply(
            NULL, "kernel32.dll", iidxhok_util_proc_mcore_hook_syms, lengthof(iidxhok_util_proc_mcore_hook_syms));
    }

    // TODO move this to proc and abstract to make it easy to set by a CPU ID (list)?
    if (true) {
        // the following requires at least a quad core CPU but can reduce micro stutters further
        // pin main thread to core 0
        // pin IO thread to core 1
        // pin all other threads to cores 2 and 3
        DWORD_PTR dwThreadAffinityMask = 1 << 0; // CPU core 0
        DWORD_PTR dwPreviousAffinityMask = SetThreadAffinityMask(main_thread, dwThreadAffinityMask);

        // TODO set priority for IO thread -> how to get IO thread handle?
        
        if (dwPreviousAffinityMask == 0)
        {
            // error handling code
            log_fatal("Setting affinity mask failed");
        }
    }

    // only valid for 9 and 10, TODO needs to go to its own module and cleaned up
//         HMODULE handle = GetModuleHandleA("ezusb.dll");

//         if (handle != NULL) {
//             hook_table_apply(
//             handle, "kernel32.dll", ezusb_hook_syms, lengthof(ezusb_hook_syms));
//         } else {
//             log_warning("Could not find ezusb.dll");
//         }

// iidxhook_util_proc_monitor_init();

    log_info("Initialized");
}