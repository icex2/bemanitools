---
phase: 02-protocol-reference-and-emulator-tests
plan: "05"
subsystem: testing

tags: [c99, mingw32, acio, icca, unit-tests, wine, bug-research, eamio-stub]

requires:
  - phase: 02-01
    provides: acioemu-pipe-test, Module.mk pattern, ACIO protocol doc
  - phase: 02-02
    provides: eamio-stub, time-stub static libraries

provides:
  - src/test/acioemu/acioemu-icca-test.c — 12 ICCA node unit tests passing under Wine
  - .planning/reference/bugs/345-popnmusic-15-18-regression.md — research artifact
  - .planning/reference/bugs/344-iidx-11-15-song-timing.md — research artifact
  - .planning/reference/bugs/351-iidx-tricoro-cn-boot.md — research artifact

affects:
  - phase-04 (bug artifacts give agents immediate context for regression diagnosis)
  - doc/protocol/acio.md (ICCA tests validate documented dispatch behavior)

tech-stack:
  added: []
  patterns:
    - "ICCA dispatch-only testing: zero-initialize struct ac_io_emu + ac_io_in_init, bypass ac_io_emu_init iohook dependency"
    - "Response recovery: dispatch_and_recv helper drains emu.in wire bytes then re-parses with ac_io_out"
    - "eamio-stub drives deterministic card/sensor/keypad state for ICCA poll path"

key-files:
  created:
    - src/test/acioemu/acioemu-icca-test.c
    - .planning/reference/bugs/345-popnmusic-15-18-regression.md
    - .planning/reference/bugs/344-iidx-11-15-song-timing.md
    - .planning/reference/bugs/351-iidx-tricoro-cn-boot.md
  modified:
    - src/test/acioemu/Module.mk (added acioemu-icca-test target)
    - Module.mk (added acioemu-icca-test.exe to tests.zip manifest)
    - dist/test/run-tests.sh (added wine ./acioemu-icca-test.exe)

key-decisions:
  - "Bypass ac_io_emu_init by zero-initializing struct ac_io_emu and calling ac_io_in_init directly — same pattern as pipe tests, no iohook dependency"
  - "dispatch_and_recv helper encodes the drain-then-reparse pattern for clean test bodies"
  - "Bug research limited to hypothesis level from git log + source reading — no gh CLI available for issue content, documented as assumption"

patterns-established:
  - "ICCA node tests derive responses via ac_io_in_drain + ac_io_out_supply round-trip"
  - "eamio-stub_reset() called at start of each ICCA state-dependent test for isolation"

requirements-completed: [AIDE-03]

duration: 13min
completed: 2026-03-01
---

# Phase 2 Plan 5: ICCA Tests and Bug Research Summary

**12 ICCA card reader node unit tests pass under Wine; three bug research artifacts with ranked hypotheses ready for Phase 4 regression diagnosis**

## Performance

- **Duration:** 13 min
- **Started:** 2026-03-01T13:11:16Z
- **Completed:** 2026-03-01T13:24:00Z
- **Tasks:** 2
- **Files created/modified:** 7

## Accomplishments

- 12 unit tests for the ACIO ICCA card reader node (`acioemu/icca.c`) derived from the protocol reference:
  - GET_VERSION with all three version variants (v150, v160, v170) — verifies minor version field
  - START_UP — verifies status 0x00 response
  - QUEUE_LOOP_START — verifies fault cleared and polling_started set
  - POLL before queue loop — verifies fault guard (status FAULT regardless of sensor state)
  - POLL idle — verifies IDLE status after queue loop with no card
  - POLL card insert — verifies GOT_UID with matching UID bytes from eamio-stub
  - POLL keypad event — verifies key_events populated on rising edge
  - POLL_FELICA for v150, v160, v170 — verifies version-specific branching behavior
- Three Phase 4 bug research artifacts with issue summary, affected games table, code path references (file:line), reproduction conditions, and ranked hypotheses with evidence

## Task Commits

1. **Task 1: ICCA node unit tests** - `14b0066` (feat)
2. **Task 2: Bug research artifacts** - `12fbed6` (docs)

## Files Created/Modified

- `src/test/acioemu/acioemu-icca-test.c` — 12 tests using dispatch_and_recv helper pattern
- `src/test/acioemu/Module.mk` — added acioemu-icca-test target with same libs as pipe-test
- `Module.mk` — added `build/bin/indep-32/acioemu-icca-test.exe` to tests.zip manifest
- `dist/test/run-tests.sh` — added `wine ./acioemu-icca-test.exe`
- `.planning/reference/bugs/345-popnmusic-15-18-regression.md` — ezusb2-popn-emu interrupt read refactor as primary hypothesis
- `.planning/reference/bugs/344-iidx-11-15-song-timing.md` — missing USB interrupt rate limiter as primary hypothesis
- `.planning/reference/bugs/351-iidx-tricoro-cn-boot.md` — missing CN filesystem redirects as primary hypothesis

## Decisions Made

- `ac_io_emu_icca_init` accepts a zero-initialized `struct ac_io_emu` — no iohook call in init itself; only `ac_io_emu_init()` calls `iohook_open_nul_fd`. Zero-init + `ac_io_in_init` is sufficient for dispatch-only tests.
- Bug research was done entirely from git log + source reading since `gh auth` was not available. Noted explicitly in each artifact as an assumption.
- `dispatch_and_recv` encapsulates the wire-drain-reparse cycle so each test body stays focused on behavior assertions, not infrastructure.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing include] Added bemanitools/eamio.h to test file**

- **Found during:** Task 1 (compile)
- **Issue:** `EAM_IO_SENSOR_FRONT`, `EAM_IO_SENSOR_BACK`, `EAM_IO_CARD_ISO15696`, `EAM_IO_KEYPAD_1` enums were not visible — they're defined in `bemanitools/eamio.h`, not in `eamio-stub.h`
- **Fix:** Added `#include "bemanitools/eamio.h"` to `acioemu-icca-test.c`
- **Commit:** `14b0066`

**2. [Rule 1 - Warning] Fixed parentheses warning in check_int_eq with bitwise OR**

- **Found during:** Task 1 (compile)
- **Issue:** `check_int_eq(resp->addr, 0x01 | AC_IO_RESPONSE_FLAG)` produced `-Wparentheses` warning due to macro expansion
- **Fix:** Cast result to `(uint8_t)` to resolve ambiguity: `check_int_eq(resp->addr, (uint8_t) (0x01 | AC_IO_RESPONSE_FLAG))`
- **Commit:** `14b0066`

## Issues Encountered

- `gh` CLI not authenticated — bug research had to rely entirely on `git log`, `git diff`, and source reading. All three artifacts document this limitation explicitly and note that issue comments/user reports could not be retrieved.
- Pre-existing `config_rc.o` windres failure causes `[BUILD] FAIL` in `aide-verify.sh` — same environmental issue as previous plans, not caused by this plan's changes. The `acioemu-icca-test.exe` binary builds and passes under Wine.

## Self-Check

- FOUND: `src/test/acioemu/acioemu-icca-test.c`
- FOUND: `.planning/reference/bugs/345-popnmusic-15-18-regression.md`
- FOUND: `.planning/reference/bugs/344-iidx-11-15-song-timing.md`
- FOUND: `.planning/reference/bugs/351-iidx-tricoro-cn-boot.md`
- FOUND: commit `14b0066` (ICCA tests)
- FOUND: commit `12fbed6` (bug artifacts)
- FOUND: `build/bin/indep-32/acioemu-icca-test.exe` (built and passes under Wine)

## Self-Check: PASSED

---
*Phase: 02-protocol-reference-and-emulator-tests*
*Completed: 2026-03-01*
