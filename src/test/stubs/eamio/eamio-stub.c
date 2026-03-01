#include "bemanitools/eamio.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "stubs/eamio/eamio-stub.h"

static uint16_t stub_keypad[2];
static uint8_t stub_sensor[2];
static uint8_t stub_card_result[2];
static uint8_t stub_card_id[2][8];

void eamio_stub_reset(void)
{
    memset(stub_keypad, 0, sizeof(stub_keypad));
    memset(stub_sensor, 0, sizeof(stub_sensor));
    memset(stub_card_result, 0, sizeof(stub_card_result));
    memset(stub_card_id, 0, sizeof(stub_card_id));
}

void eamio_stub_set_keypad(uint8_t unit_no, uint16_t state)
{
    if (unit_no < 2) {
        stub_keypad[unit_no] = state;
    }
}

void eamio_stub_set_sensor(uint8_t unit_no, uint8_t state)
{
    if (unit_no < 2) {
        stub_sensor[unit_no] = state;
    }
}

void eamio_stub_set_card(
    uint8_t unit_no,
    uint8_t result,
    const uint8_t *card_id,
    uint8_t nbytes)
{
    if (unit_no < 2) {
        stub_card_result[unit_no] = result;

        if (card_id != NULL && nbytes > 0) {
            uint8_t copy = nbytes < 8 ? nbytes : 8;
            memcpy(stub_card_id[unit_no], card_id, copy);
        }
    }
}

/* eamio API implementation */

void eam_io_set_loggers(
    log_formatter_t misc,
    log_formatter_t info,
    log_formatter_t warning,
    log_formatter_t fatal)
{
    (void) misc;
    (void) info;
    (void) warning;
    (void) fatal;
}

bool eam_io_init(
    thread_create_t thread_create,
    thread_join_t thread_join,
    thread_destroy_t thread_destroy)
{
    (void) thread_create;
    (void) thread_join;
    (void) thread_destroy;

    return true;
}

void eam_io_fini(void)
{
}

uint16_t eam_io_get_keypad_state(uint8_t unit_no)
{
    if (unit_no < 2) {
        return stub_keypad[unit_no];
    }

    return 0;
}

uint8_t eam_io_get_sensor_state(uint8_t unit_no)
{
    if (unit_no < 2) {
        return stub_sensor[unit_no];
    }

    return 0;
}

uint8_t eam_io_read_card(uint8_t unit_no, uint8_t *card_id, uint8_t nbytes)
{
    if (unit_no < 2 && card_id != NULL && nbytes > 0) {
        uint8_t copy = nbytes < 8 ? nbytes : 8;
        memcpy(card_id, stub_card_id[unit_no], copy);
    }

    if (unit_no < 2) {
        return stub_card_result[unit_no];
    }

    return 0;
}

bool eam_io_card_slot_cmd(uint8_t unit_no, uint8_t cmd)
{
    (void) unit_no;
    (void) cmd;

    return true;
}

bool eam_io_poll(uint8_t unit_no)
{
    (void) unit_no;

    return true;
}

const struct eam_io_config_api *eam_io_get_config_api(void)
{
    return NULL;
}
