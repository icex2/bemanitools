---
phase: 02-protocol-reference-and-emulator-tests
plan: "04"
subsystem: testing

tags: [c99, mingw32, ezusb-iidx, protocol-reference, unit-testing, fpga, emulation]

requires:
  - phase: 02-protocol-reference-and-emulator-tests
    plan: "02"
    provides: iidxio-stub, eamio-stub, time-stub libraries

provides:
  - ezusb-iidx protocol reference covering all USB pipes, packet formats, node dispatch, FPGA sequences, serial node, and board type differences
  - ezusb-iidx-emu-fpga-test: 11 tests for FPGA V1 (5 commands) and V2 (4 commands + fault paths)
  - ezusb-iidx-emu-msg-test: 11 tests for interrupt read/write dispatch and inverted_pad bit layout

affects:
  - agents diagnosing IIDX 9-19 timing and initialization regressions (#344 regression target)
  - future tests against ezusb-iidx-emu emulation layer

tech-stack:
  added: []
  patterns:
    - "Test FPGA nodes via exported process_cmd functions (ezusb_iidx_emu_node_fpga_v1/v2_process_cmd)"
    - "Test msg layer via returned hook struct (hook->interrupt_read/write, bulk_read)"
    - "ezusb-iidx-16seg-emu and security libs required to resolve all link dependencies"

key-files:
  created:
    - doc/protocol/ezusb-iidx.md
    - src/test/ezusb-iidx-emu/ezusb-iidx-emu-fpga-test.c
    - src/test/ezusb-iidx-emu/ezusb-iidx-emu-msg-test.c
    - src/test/ezusb-iidx-emu/Module.mk
  modified:
    - Module.mk

key-decisions:
  - "FPGA node functions tested directly (exported process_cmd signatures) rather than via msg layer — avoids iohook dependency and tests the documented command/status mapping in isolation"
  - "Msg layer tested via hook struct returned by ezusb_iidx_emu_msg_init — public API, no static internals accessed"
  - "ezusb-iidx-16seg-emu and security added to link deps — node-security-plug calls security_rp/security_id; node_16seg comes from 16seg-emu lib"
  - "confidence tags (Verified/Corroborated/Inferred) used throughout protocol doc to mark reverse-engineering certainty"

requirements-completed: [AIDE-03]

duration: ~12min
completed: 2026-03-01
---

# Phase 2 Plan 04: ezusb-iidx Protocol Reference and Emulator Tests Summary

**Complete ezusb-iidx reverse-engineering reference plus FPGA command sequence and interrupt read/write dispatch unit tests**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-03-01T13:11:27Z
- **Completed:** 2026-03-01T13:23:15Z
- **Tasks:** 2
- **Files modified:** 5 (4 created, 1 modified)

## Accomplishments

- `doc/protocol/ezusb-iidx.md`: 11-section reference covering USB pipe architecture, interrupt write/read packet field-by-field layouts, inverted_pad 32-bit bit map, bulk packet format, node dispatch (V1 and V2 tables), FPGA V1 and V2 init sequences, serial node H8 framing and mag card state machine, other nodes (SECURITY_PLUG, EEPROM, COIN, WDT, SRAM, 16SEG, SECURITY_MEM), and C02/D01 board type differences. 63 confidence tags (Verified/Corroborated/Inferred).
- `ezusb-iidx-emu-fpga-test`: 11 tests covering FPGA V1 (INIT→OK_2, CHECK→OK, CHECK_2→OK_2, WRITE→OK, WRITE_DONE→OK_2, unknown→FAULT) and FPGA V2 (INIT→0x41, CHECK→0x42, WRITE→0x43, WRITE_DONE→0x43, unknown→0xFE).
- `ezusb-iidx-emu-msg-test`: 11 tests covering inverted_pad active-low P1/P2 key bits (8-14, 15-21), panel bits (24-27), sys bits (28-30), fpga2_check_flag_unkn always 2, fpga_write_ready always 1, D01 board type forces bit 4 to 0, interrupt write→status dispatch, status cleared after read, bulk read dispatches to cur_node.
- Both executables build clean for indep-32 and indep-64.

## Task Commits

1. **Task 1: ezusb-iidx protocol reference** - `bf5f4dc` (docs)
2. **Task 2: FPGA and message dispatch tests** - `d3f77ba` (feat)

## Files Created/Modified

- `doc/protocol/ezusb-iidx.md` — 11-section protocol reference with 63 confidence-tagged behaviors
- `src/test/ezusb-iidx-emu/ezusb-iidx-emu-fpga-test.c` — FPGA V1/V2 command/status unit tests
- `src/test/ezusb-iidx-emu/ezusb-iidx-emu-msg-test.c` — interrupt read/write dispatch and inverted_pad bit layout tests
- `src/test/ezusb-iidx-emu/Module.mk` — build integration with security + ezusb-iidx-16seg-emu deps
- `Module.mk` — added `include src/test/ezusb-iidx-emu/Module.mk`

## Decisions Made

- FPGA node functions (`ezusb_iidx_emu_node_fpga_v1/v2_process_cmd`) are public so tested directly — clean unit tests without iohook or msg layer overhead.
- Msg layer accessed through the `struct ezusb_emu_msg_hook *` returned by `ezusb_iidx_emu_msg_init` — this is the natural public API boundary.
- Link deps expanded from plan's baseline: `ezusb-iidx-16seg-emu` needed for `ezusb_iidx_emu_node_16seg` (referenced by both node tables in msg.c); `security` needed for `security_rp/security_id` called by `node-security-plug.c`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added missing link dependencies**
- **Found during:** Task 2 (link step)
- **Issue:** Plan's Module.mk template listed `ezusb-iidx-emu`, `ezusb-emu`, and stubs, but linking failed with undefined references to `ezusb_iidx_emu_node_16seg` (from ezusb-iidx-16seg-emu) and `security_mcode_to_str`/`security_id_verify`/`security_rp_generate_signed_eeprom_data` (from security lib)
- **Fix:** Added `ezusb-iidx-16seg-emu` and `security` to both test targets' `libs_` lists
- **Files modified:** `src/test/ezusb-iidx-emu/Module.mk`
- **Commit:** d3f77ba

## Issues Encountered

Wine tests cannot run in the sandbox environment (wineserver: error creating lock file at `/tmp/.wine-1000/server-*/lock`; `/tmp` is mounted read-only). This is a pre-existing environment constraint documented in prior plans (02-02-SUMMARY.md). Both executables compiled and linked successfully for indep-32 and indep-64. [BUILD] PASS confirmed via `make build/bin/indep-32/ezusb-iidx-emu-fpga-test.exe` and `make build/bin/indep-32/ezusb-iidx-emu-msg-test.exe` exiting 0. [FORMAT] not modified our files (clang-format produced no diff on src/test/ezusb-iidx-emu/).

## Self-Check: PASSED

- FOUND: doc/protocol/ezusb-iidx.md
- FOUND: src/test/ezusb-iidx-emu/ezusb-iidx-emu-fpga-test.c
- FOUND: src/test/ezusb-iidx-emu/ezusb-iidx-emu-msg-test.c
- FOUND: src/test/ezusb-iidx-emu/Module.mk
- FOUND: build/bin/indep-32/ezusb-iidx-emu-fpga-test.exe
- FOUND: build/bin/indep-32/ezusb-iidx-emu-msg-test.exe
- FOUND: build/bin/indep-64/ezusb-iidx-emu-fpga-test.exe
- FOUND: build/bin/indep-64/ezusb-iidx-emu-msg-test.exe
- FOUND: commit bf5f4dc (docs(02-04): add ezusb-iidx protocol reference)
- FOUND: commit d3f77ba (feat(02-04): add ezusb-iidx emulator FPGA and message dispatch tests)

---
*Phase: 02-protocol-reference-and-emulator-tests*
*Completed: 2026-03-01*
