#ifndef IIDXIO_STUB_H
#define IIDXIO_STUB_H

#include <stdint.h>

/* Reset all stub state to zero */
void iidxio_stub_reset(void);

/* Configure input state returned by iidx_io_ep2_get_keys */
void iidxio_stub_set_keys(uint16_t keys);

/* Configure input state returned by iidx_io_ep2_get_panel */
void iidxio_stub_set_panel(uint8_t panel);

/* Configure input state returned by iidx_io_ep2_get_sys */
void iidxio_stub_set_sys(uint8_t sys);

/* Configure turntable position for player 0 or 1 */
void iidxio_stub_set_turntable(uint8_t player_no, uint8_t value);

/* Configure slider position for slider 0..4 */
void iidxio_stub_set_slider(uint8_t slider_no, uint8_t value);

#endif
