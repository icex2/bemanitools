#include "util/time.h"

#include <stdint.h>

#include "stubs/time/time-stub.h"

static uint64_t stub_counter;
static uint64_t stub_elapsed_ns;
static uint64_t stub_elapsed_us;
static uint32_t stub_elapsed_ms;

void time_stub_reset(void)
{
    stub_counter = 0;
    stub_elapsed_ns = 1000000000;
    stub_elapsed_us = 1000000;
    stub_elapsed_ms = 1000;
}

void time_stub_set_counter(uint64_t value)
{
    stub_counter = value;
}

void time_stub_set_elapsed(uint64_t ns, uint64_t us, uint32_t ms)
{
    stub_elapsed_ns = ns;
    stub_elapsed_us = us;
    stub_elapsed_ms = ms;
}

/* time API implementation */

uint64_t time_get_counter(void)
{
    return stub_counter;
}

uint64_t time_get_elapsed_ns(uint64_t counter_delta)
{
    (void) counter_delta;

    return stub_elapsed_ns;
}

uint64_t time_get_elapsed_us(uint64_t counter_delta)
{
    (void) counter_delta;

    return stub_elapsed_us;
}

uint32_t time_get_elapsed_ms(uint64_t counter_delta)
{
    (void) counter_delta;

    return stub_elapsed_ms;
}
