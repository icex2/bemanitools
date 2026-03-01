#include "bemanitools/iidxio.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "stubs/iidxio/iidxio-stub.h"

static uint16_t stub_keys;
static uint8_t stub_panel;
static uint8_t stub_sys;
static uint8_t stub_turntable[2];
static uint8_t stub_slider[5];

void iidxio_stub_reset(void)
{
    stub_keys = 0;
    stub_panel = 0;
    stub_sys = 0;
    memset(stub_turntable, 0, sizeof(stub_turntable));
    memset(stub_slider, 0, sizeof(stub_slider));
}

void iidxio_stub_set_keys(uint16_t keys)
{
    stub_keys = keys;
}

void iidxio_stub_set_panel(uint8_t panel)
{
    stub_panel = panel;
}

void iidxio_stub_set_sys(uint8_t sys)
{
    stub_sys = sys;
}

void iidxio_stub_set_turntable(uint8_t player_no, uint8_t value)
{
    if (player_no < 2) {
        stub_turntable[player_no] = value;
    }
}

void iidxio_stub_set_slider(uint8_t slider_no, uint8_t value)
{
    if (slider_no < 5) {
        stub_slider[slider_no] = value;
    }
}

/* iidxio API implementation */

void iidx_io_set_loggers(
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

bool iidx_io_init(
    thread_create_t thread_create,
    thread_join_t thread_join,
    thread_destroy_t thread_destroy)
{
    (void) thread_create;
    (void) thread_join;
    (void) thread_destroy;

    return true;
}

void iidx_io_fini(void)
{
}

void iidx_io_ep1_set_deck_lights(uint16_t deck_lights)
{
    (void) deck_lights;
}

void iidx_io_ep1_set_panel_lights(uint8_t panel_lights)
{
    (void) panel_lights;
}

void iidx_io_ep1_set_top_lamps(uint8_t top_lamps)
{
    (void) top_lamps;
}

void iidx_io_ep1_set_top_neons(bool top_neons)
{
    (void) top_neons;
}

bool iidx_io_ep1_send(void)
{
    return true;
}

bool iidx_io_ep2_recv(void)
{
    return true;
}

uint8_t iidx_io_ep2_get_turntable(uint8_t player_no)
{
    if (player_no < 2) {
        return stub_turntable[player_no];
    }

    return 0;
}

uint8_t iidx_io_ep2_get_slider(uint8_t slider_no)
{
    if (slider_no < 5) {
        return stub_slider[slider_no];
    }

    return 0;
}

uint8_t iidx_io_ep2_get_sys(void)
{
    return stub_sys;
}

uint8_t iidx_io_ep2_get_panel(void)
{
    return stub_panel;
}

uint16_t iidx_io_ep2_get_keys(void)
{
    return stub_keys;
}

bool iidx_io_ep3_write_16seg(const char *text)
{
    (void) text;

    return true;
}
