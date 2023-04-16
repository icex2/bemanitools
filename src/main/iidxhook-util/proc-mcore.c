#define LOG_MODULE "iidxhook-util-proc-mcore"

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

static int iidxhook_util_proc_mcore_thread_priority_blocklist[3];

static BOOL STDCALL my_SetThreadPriority(
        HANDLE hThread,
        int nPriority)
{
    int thread_id;

    thread_id = GetThreadId(hThread);

    for (int i = 0; i < sizeof(iidxhook_util_proc_mcore_thread_priority_blocklist); i++) {
        if (thread_id == iidxhook_util_proc_mcore_thread_priority_blocklist[i]) {
            // Block changes
            return TRUE;
        }     
    }

    return real_SetThreadPriority(hThread, nPriority);
}

static uint8_t iidxhook_util_proc_mcore_get_hw_cpu_count()
{
    SYSTEM_INFO sysInfo;
    
    GetSystemInfo(&sysInfo);

    return sysInfo.dwNumberOfProcessors;
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

static void patch_ezusb_threads()
{
    size_t count;
    struct proc_thread_info* thread_infos;
    char origin_module_name[MAX_PATH];

    count = iidxhook_util_proc_mcore_find_ezusb_io_threads(&thread_infos);
    
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

        if (!proc_thread_set_priority(thread_infos[i].id, THREAD_PRIORITY_TIME_CRITICAL)) {
            log_fatal("Setting thread priority to time critical for thread %d failed", thread_infos[i].id);
        }

        if (!proc_thread_set_affinity(thread_infos[i].id, 1)) {
            log_fatal("Setting thread affinity to cpu core 1 for thread %d failed", thread_infos[i].id);
        }

        iidxhook_util_proc_mcore_thread_priority_blocklist[i] = thread_infos[i].id;
    }
}

static void patch_main_thread(int thread_id)
{
    if (!proc_thread_set_priority(thread_id, THREAD_PRIORITY_TIME_CRITICAL)) {
        log_fatal("Setting main thread priority to time critical for thread %d failed", thread_id);
    }

    if (!proc_thread_set_affinity(thread_id, 0)) {
        log_fatal("Setting main thread affinity to cpu core 0 for thread %d failed", thread_id);
    }

    iidxhook_util_proc_mcore_thread_priority_blocklist[2] = thread_id;
}

void iidxhook_util_proc_mcore_init(
        int main_thread_id,
        enum IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES cpu_cores)
{
    uint8_t cpu_core_count;
    
    // patch_ezusb_threads();
    patch_main_thread(main_thread_id);

    hook_table_apply(
            NULL, "kernel32.dll", iidxhok_util_proc_mcore_hook_syms, lengthof(iidxhok_util_proc_mcore_hook_syms));

    // switch (cpu_cores) {
    //     case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_AUTO:
    //         iidxhook_util_proc_mcore_get_hw_cpu_count();

    //         // TODO check return value and either go for 2 or 4 core optimization

    //         break;

    //     case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_2:
    //         cpu_core_count = 2;
    //     case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_4:
    //         cpu_core_count = 4;
    //         break; 
    //     case IIDXHOOK_UTIL_PROC_MCORE_CPU_CORES_INVALID:
    //     default:
    //         log_fatal("Illegal state, value: %d", cpu_cores);
    //         break;
    // }

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