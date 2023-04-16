#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct proc_thread_info {
    int id;
    void* proc;
    // According to
    // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadpriority#return-value
    int priority;
    HMODULE origin_module;
};

void proc_terminate_current_process(uint32_t exit_code);

void* proc_thread_get_proc_address(int thread_id);

HMODULE proc_thread_proc_get_origin_module(void* proc_addr);

bool proc_thread_proc_get_origin_module_path(void* proc_addr, char* buffer, size_t len);

bool proc_thread_proc_get_origin_module_name(void* proc_addr, char* buffer, size_t len);

size_t proc_thread_scan_threads_current_process(struct proc_thread_info** info);

HANDLE proc_thread_get_handle(int thread_id);