#define LOG_MODULE "proc-mon"

#include <windows.h>

#include "hook/table.h"

#include "util/log.h"
#include "util/mem.h"
#include "util/time.h"

static DWORD STDCALL my_GetVersion();
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
static BOOL STDCALL my_TerminateThread(
    HANDLE hThread,
    DWORD dwExitCode);
static DWORD STDCALL my_ResumeThread(
    HANDLE hThread);
static void STDCALL my_ExitThread(
    DWORD dwExitCode);

static DWORD (STDCALL *real_GetVersion)();
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
static BOOL (STDCALL *real_TerminateThread)(
    HANDLE hThread,
    DWORD dwExitCode);
static DWORD (STDCALL *real_ResumeThread)(
    HANDLE hThread);
static void (STDCALL *real_ExitThread)(
    DWORD dwExitCode);

static const struct hook_symbol iidxhook_util_proc_monitor_hook_syms[] = {
    {
        .name = "GetVersion",
        .patch = my_GetVersion,
        .link = (void **) &real_GetVersion,
    },
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
    {
        .name = "TerminateThread",
        .patch = my_TerminateThread,
        .link = (void **) &real_TerminateThread,
    },
    {
        .name = "ResumeThread",
        .patch = my_ResumeThread,
        .link = (void **) &real_ResumeThread,
    },
    {
        .name = "ExitThread",
        .patch = my_ExitThread,
        .link = (void **) &real_ExitThread,
    },
};

static HANDLE iidxhook_util_proc_mon_main_thread_handle = INVALID_HANDLE_VALUE;

struct iidxhook_proc_mon_thread_proc_monitor_data
{
    LPVOID original_parameters;
    LPTHREAD_START_ROUTINE original_thread_proc;
};

static DWORD STDCALL iidxhook_proc_mon_thread_proc_monitor(LPVOID lpParameter)
{
    struct iidxhook_proc_mon_thread_proc_monitor_data *proc_data;
    DWORD proc_return_value;
    HMODULE module_handle;
    char module_name[MAX_PATH];
    DWORD thread_id;
    HANDLE thread_handle;
    int64_t duration_start;
    double duration_secs;

    log_assert(lpParameter);

    proc_data = (struct iidxhook_proc_mon_thread_proc_monitor_data*) lpParameter;

    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCTSTR)proc_data->original_thread_proc, &module_handle);
    GetModuleFileNameA(module_handle, module_name, sizeof(module_name));
    thread_id = GetCurrentThreadId();
    thread_handle = GetCurrentThread();

    // TODO measure duration

    log_misc("THREAD_PROC_START(module_handle %p, module_name %s, thread_id %ld, thread_handle %p, lpStartAddress %p, lpParameter %p)",
        module_handle, module_name, thread_id, thread_handle, proc_data->original_thread_proc, proc_data->original_parameters);

    duration_start = time_get_counter();

    proc_return_value = proc_data->original_thread_proc(proc_data->original_parameters);

    duration_secs = ((double) time_get_elapsed_ns(duration_start)) / 1000.0 / 1000.0 / 1000.0;

    log_misc("THREAD_PROC_END(module_handle %p, module_name %s, thread_id %ld, thread_handle %p, lpStartAddress %p, lpParameter %p): result %ld, duration %f secs",
         module_handle, module_name, thread_id, thread_handle, proc_data->original_thread_proc, proc_data->original_parameters, proc_return_value,
        duration_secs);

    free(proc_data);

    return proc_return_value;
}

static DWORD STDCALL my_GetVersion()
{
    // First function called in _start of the main executable module
    // Use this as an entry point to capture/monitor the main thread

    HANDLE module_handle;
    char module_name[MAX_PATH];
    DWORD thread_id;

    module_handle = GetModuleHandleA(NULL);
    GetModuleFileNameA(module_handle, module_name, sizeof(module_name));
    thread_id = GetCurrentThreadId();
    iidxhook_util_proc_mon_main_thread_handle = GetCurrentThread();

    log_misc("MAIN_THREAD_START(module_handle %p, module_name %s, thread_id %ld, thread_handle %p)", module_handle, module_name, thread_id, iidxhook_util_proc_mon_main_thread_handle);

    return real_GetVersion();
}

static HANDLE STDCALL my_CreateThread(
        LPSECURITY_ATTRIBUTES lpThreadAttributes,
        SIZE_T dwStackSize,
        LPTHREAD_START_ROUTINE lpStartAddress,
        __drv_aliasesMem LPVOID lpParameter,
        DWORD dwCreationFlags,
        LPDWORD lpThreadId)
{
    HANDLE handle;
    HMODULE module_handle;
    char module_name[MAX_PATH];
    DWORD current_thread_id;
    struct iidxhook_proc_mon_thread_proc_monitor_data *proc_data;

    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCTSTR)lpStartAddress, &module_handle);
    GetModuleFileNameA(module_handle, module_name, sizeof(module_name));
    current_thread_id = GetCurrentThreadId();

    log_misc("CreateThread(module_handle %p, module_name %s, current_thread_id %ld, lpThreadAttributes %p, dwStackSize %Iu, lpStartAddress %p, lpParameter %p, dwCreationFlags 0x%lx, lpThreadId %p)",
        module_handle,
        module_name,
        current_thread_id,
        lpThreadAttributes,
        dwStackSize,
        lpStartAddress,
        lpParameter,
        dwCreationFlags,
        lpThreadId);

    proc_data = xmalloc(sizeof(struct iidxhook_proc_mon_thread_proc_monitor_data));

    proc_data->original_parameters = lpParameter;
    proc_data->original_thread_proc = lpStartAddress;

    handle = real_CreateThread(lpThreadAttributes, dwStackSize, iidxhook_proc_mon_thread_proc_monitor, proc_data, dwCreationFlags, lpThreadId);

    log_misc("CreateThread(module_handle %p, module_name %s, current_thread_id %ld, lpThreadAttributes %p, dwStackSize %Iu, lpStartAddress %p, lpParameter %p, dwCreationFlags 0x%lx, lpThreadId %p): %p",
        module_handle,
        module_name,
        current_thread_id,
        lpThreadAttributes,
        dwStackSize,
        lpStartAddress,
        lpParameter,
        dwCreationFlags,
        lpThreadId,
        handle);

    return handle;
}

static BOOL STDCALL my_SetThreadPriority(
        HANDLE hThread,
        int nPriority)
{
    BOOL res;
    HANDLE module_handle;
    char module_name[MAX_PATH];
    DWORD thread_id;

    module_handle = GetModuleHandleA(NULL);
    GetModuleFileNameA(module_handle, module_name, sizeof(module_name));
    thread_id = GetThreadId(hThread);

    log_misc("SetThreadPriority(module_handle %p, module_name %s, thread_id %ld, hThread %p, nPriority %d)", module_handle, module_name, thread_id, hThread, nPriority);

    res = real_SetThreadPriority(hThread, nPriority);

    log_misc("SetThreadPriority(module_handle %p, module_name %s, thread_id %ld, hThread %p, nPriority %d): %d", module_handle, module_name, thread_id, hThread, nPriority, res);

    return res;
}

static BOOL STDCALL my_TerminateThread(
        HANDLE hThread,
        DWORD dwExitCode)
{
    BOOL res;

    log_misc("TerminateThread(hThread %p, dwExitCode %ld)", hThread, dwExitCode);

    res = real_TerminateThread(hThread, dwExitCode);

    log_misc("TerminateThread(hThread %p, dwExitCode %ld): %d", hThread, dwExitCode, res);

    return res;
}

static DWORD STDCALL my_ResumeThread(
        HANDLE hThread)
{
    DWORD res;

    log_misc("ResumeThread(hThread %p)", hThread);

    res = real_ResumeThread(hThread);

    log_misc("ResumeThread(hThread %p): %ld", hThread, res);

    return res;
}

static void STDCALL my_ExitThread(
        DWORD dwExitCode)
{
    DWORD thread_id;
    HANDLE thread_handle;

    thread_id = GetCurrentThreadId();
    thread_handle = GetCurrentThread();

    log_misc("ExitThread(thread_id %ld, thread_handle %p, dwExitCode %ld)", thread_id, thread_handle, dwExitCode);

    real_ExitThread(dwExitCode);
}

void iidxhook_util_proc_monitor_init()
{
    hook_table_apply(
            NULL, "kernel32.dll", iidxhook_util_proc_monitor_hook_syms, lengthof(iidxhook_util_proc_monitor_hook_syms));

// only valid for 9 and 10, TODO needs to go to its own module and cleaned up
    // HMODULE handle = GetModuleHandleA("ezusb.dll");

    // if (handle != NULL) {
    //     hook_table_apply(
    //     handle, "kernel32.dll", iidxhook_util_proc_monitor_hook_syms, lengthof(iidxhook_util_proc_monitor_hook_syms));
        
    // } else {
    //     log_warning("Could not find ezusb.dll");
    // }

    

    log_info("Initialized");
}

HANDLE iidxhook_util_proc_monitor_get_main_thread_handle()
{
    log_assert(iidxhook_util_proc_mon_main_thread_handle != INVALID_HANDLE_VALUE);

    return iidxhook_util_proc_mon_main_thread_handle;
}