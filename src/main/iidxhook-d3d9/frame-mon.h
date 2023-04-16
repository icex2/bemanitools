#ifndef IIDXHOOK_D3D9_FRAME_MON_H
#define IIDXHOOK_D3D9_FRAME_MON_H

#include "hook/d3d9.h"

void iidxhook_d3d9_frame_mon_init(
        double target_frame_rate_hz,
        float log_slow_frames_margin,
        float log_fast_frames_margin,
        const char* frame_stats_out_path);

HRESULT iidxhook_d3d9_frame_mon_d3d9_irp_handler(struct hook_d3d9_irp *irp);

#endif