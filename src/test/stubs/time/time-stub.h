#ifndef TIME_STUB_H
#define TIME_STUB_H

#include <stdint.h>

/* Reset all stub state to defaults: counter=0, elapsed=1 second */
void time_stub_reset(void);

/* Set the value returned by time_get_counter */
void time_stub_set_counter(uint64_t value);

/* Set all elapsed-time return values at once for consistency */
void time_stub_set_elapsed(uint64_t ns, uint64_t us, uint32_t ms);

#endif
