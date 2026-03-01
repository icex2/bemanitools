#ifndef EAMIO_STUB_H
#define EAMIO_STUB_H

#include <stdint.h>

/* Reset all stub state to zero */
void eamio_stub_reset(void);

/* Configure keypad state returned by eam_io_get_keypad_state for unit 0 or 1 */
void eamio_stub_set_keypad(uint8_t unit_no, uint16_t state);

/* Configure sensor state returned by eam_io_get_sensor_state for unit 0 or 1
 */
void eamio_stub_set_sensor(uint8_t unit_no, uint8_t state);

/* Configure card data returned by eam_io_read_card for unit 0 or 1 */
void eamio_stub_set_card(
    uint8_t unit_no,
    uint8_t result,
    const uint8_t *card_id,
    uint8_t nbytes);

#endif
