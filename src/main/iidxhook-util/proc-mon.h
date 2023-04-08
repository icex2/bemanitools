#ifndef IIDXHOOK_PROC_MON_H
#define IIDXHOOK_PROC_MON_H

#include <windows.h>

void iidxhook_util_proc_monitor_init();

HANDLE iidxhook_util_proc_monitor_get_main_thread_handle();

#endif