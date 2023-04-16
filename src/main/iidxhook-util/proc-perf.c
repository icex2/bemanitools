#define LOG_MODULE "iidxhook-util-proc-perf"

#include "hook/table.h"

#include "util/defs.h"
#include "util/log.h"

#include "proc-perf.h"

static BOOL STDCALL my_SetThreadPriority(
    HANDLE hThread,
    int nPriority);
static BOOL (STDCALL *real_SetThreadPriority)(
    HANDLE hThread,
    int nPriority);

static const struct hook_symbol iidxhok_util_proc_perf_hook_syms[] = {
    {
        .name = "SetThreadPriority",
        .patch = my_SetThreadPriority,
        .link = (void **) &real_SetThreadPriority,
    },
};

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
        log_info("Patch main thread, realtime priority");
        return real_SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);
    } else {
        // TODO ???
        //log_info("Patch thread priority of thread %p, priority %d -> 0 (normal)", hThread, nPriority);
    }   

    // TODO what about patching the ezusb IO threads on 9 and 10 with separate ezusb lib?
   
    return real_SetThreadPriority(hThread, nPriority);
}

void iidxhook_util_proc_perf_init(HANDLE main_thread_handle)
{
    iidxhook_util_proc_perf_main_thread_handle = main_thread_handle;

    // Boost main thread prio to max to ensure the render loop is prioritzed on
    // thread scheduling which reduces microstutters
    SetThreadPriority(main_thread_handle, THREAD_PRIORITY_TIME_CRITICAL);

    // the following requires at least a quad core CPU but can reduce micro stutters further
    // pin main thread to core 0
    // pin IO thread to core 1
    // pin all other threads to cores 2 and 3
    DWORD_PTR dwThreadAffinityMask = 1 << 0; // CPU core 0
    DWORD_PTR dwPreviousAffinityMask = SetThreadAffinityMask(main_thread_handle, dwThreadAffinityMask);

    // TODO set priority for IO thread -> how to get IO thread handle?
    
    if (dwPreviousAffinityMask == 0)
    {
        // error handling code
        log_fatal("Setting affinity mask failed");
    }

    hook_table_apply(
            NULL, "kernel32.dll", iidxhok_util_proc_perf_hook_syms, lengthof(iidxhok_util_proc_perf_hook_syms));




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