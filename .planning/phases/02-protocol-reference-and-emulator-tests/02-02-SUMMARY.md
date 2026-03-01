---
phase: 02-protocol-reference-and-emulator-tests
plan: "02"
subsystem: testing

tags: [c99, mingw32, static-library, test-stubs, iidxio, eamio, unit-testing]

requires:
  - phase: 01-agent-foundation
    provides: build system and aide-verify.sh verification infrastructure

provides:
  - iidxio-stub static library with configurable state for deterministic tests
  - eamio-stub static library with configurable state for deterministic tests
  - time-stub static library with settable counter/elapsed values for deterministic tests
  - Stub include pattern: test code includes stubs/X/X-stub.h via -I src/test path

affects:
  - phase 02 emulator tests (bio2emu-iidx, acioemu, ezusb-iidx-emu)
  - any future test that needs iidxio, eamio, or time dependencies

tech-stack:
  added: []
  patterns:
    - "Test stubs as static libs in src/test/stubs/, included via Module.mk in root Module.mk"
    - "Stub headers under stubs/X/ reachable via -I src/test include path"
    - "All unused params suppressed with (void) param; following project convention"
    - "Setter functions prefixed with module_stub_ (e.g. iidxio_stub_set_keys)"

key-files:
  created:
    - src/test/stubs/iidxio/iidxio-stub.c
    - src/test/stubs/iidxio/iidxio-stub.h
    - src/test/stubs/iidxio/Module.mk
    - src/test/stubs/eamio/eamio-stub.c
    - src/test/stubs/eamio/eamio-stub.h
    - src/test/stubs/eamio/Module.mk
    - src/test/stubs/time/time-stub.c
    - src/test/stubs/time/time-stub.h
    - src/test/stubs/time/Module.mk
  modified:
    - Module.mk

key-decisions:
  - "Stub headers placed under src/test/stubs/X/ and included as stubs/X/X-stub.h via -I src/test (consistent with how test/ headers are included)"
  - "time-stub defaults: elapsed=1s (1000ms/1000000us/1000000000ns) to make all queued responses immediately available in drain logic"
  - "Bounds checking in get/set functions (player_no < 2, slider_no < 5) for correctness without assertions"

patterns-established:
  - "Stub Module.mk adds to libs += list, not testexes — stubs are linkable archives, not executables"
  - "Include the real API header in the stub .c file to enforce signature match at compile time"

requirements-completed: [AIDE-03]

duration: 4min
completed: 2026-03-01
---

# Phase 2 Plan 02: Test Stub Libraries Summary

**Three static stub libraries (iidxio-stub, eamio-stub, time-stub) with configurable state for deterministic emulator unit tests**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-01T23:02:00Z
- **Completed:** 2026-03-01T23:06:43Z
- **Tasks:** 2
- **Files modified:** 10 (9 created, 1 modified)

## Accomplishments

- iidxio-stub: full iidxio API with setter functions for keys, panel, sys, turntable, slider state
- eamio-stub: full eamio API with setter functions for keypad, sensor, and card read state
- time-stub: deterministic time API returning configurable counter and elapsed values (defaults to 1s elapsed)
- All three wired into root Module.mk, compile clean for both indep-32 and indep-64 targets

## Task Commits

1. **Task 1: iidxio-stub and eamio-stub libraries** - `8f37c11` (feat)
2. **Task 2: time-stub library and Module.mk integration** - `257f7f0` (feat)

## Files Created/Modified

- `src/test/stubs/iidxio/iidxio-stub.c` - iidxio API stub; all functions no-op except getters returning stub state
- `src/test/stubs/iidxio/iidxio-stub.h` - Setter API: iidxio_stub_reset/set_keys/set_panel/set_sys/set_turntable/set_slider
- `src/test/stubs/iidxio/Module.mk` - Adds iidxio-stub to libs
- `src/test/stubs/eamio/eamio-stub.c` - eamio API stub; getters return stub state, card read copies stub card data
- `src/test/stubs/eamio/eamio-stub.h` - Setter API: eamio_stub_reset/set_keypad/set_sensor/set_card
- `src/test/stubs/eamio/Module.mk` - Adds eamio-stub to libs
- `src/test/stubs/time/time-stub.c` - time API stub; all functions return configurable values, ignores counter_delta
- `src/test/stubs/time/time-stub.h` - Setter API: time_stub_reset/set_counter/set_elapsed
- `src/test/stubs/time/Module.mk` - Adds time-stub to libs
- `Module.mk` - Added three include lines for the stub modules

## Decisions Made

- Stub headers use path `stubs/X/X-stub.h` (resolved via `-I src/test`), consistent with how test framework header `test/check.h` is included in the existing codebase.
- time-stub defaults 1s elapsed so drain loops in emulator state machines see all queued responses as immediately ready — no need to configure timing for basic flow tests.
- Bounds checks in getters/setters (player_no < 2, slider_no < 5) return 0 for out-of-range rather than asserting; tests don't need to handle panics.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

Wine tests cannot run in the sandbox environment (wineserver: error creating lock file on read-only filesystem). This is a pre-existing environment constraint, not caused by these changes. [BUILD] PASS and [FORMAT] SKIP confirmed. Stubs compiled cleanly for indep-32 and indep-64 with zero warnings.

## Next Phase Readiness

All three stub libraries are available for test executables to link against. The next plan can create actual test executables for bio2emu-iidx, acioemu, or ezusb-iidx-emu by adding `iidxio-stub`, `eamio-stub`, and/or `time-stub` to their `libs_` lists in their Module.mk files.

## Self-Check: PASSED

- FOUND: src/test/stubs/iidxio/iidxio-stub.c
- FOUND: src/test/stubs/eamio/eamio-stub.c
- FOUND: src/test/stubs/time/time-stub.c
- FOUND: build/obj/indep-32/iidxio-stub/iidxio-stub.o
- FOUND: build/obj/indep-32/eamio-stub/eamio-stub.o
- FOUND: build/obj/indep-32/time-stub/time-stub.o
- FOUND: commit 8f37c11 (iidxio-stub and eamio-stub)
- FOUND: commit 257f7f0 (time-stub and Module.mk integration)

---
*Phase: 02-protocol-reference-and-emulator-tests*
*Completed: 2026-03-01*
