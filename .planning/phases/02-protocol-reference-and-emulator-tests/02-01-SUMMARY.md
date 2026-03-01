---
phase: 02-protocol-reference-and-emulator-tests
plan: "01"
subsystem: testing
tags: [acio, protocol, reverse-engineering, unit-tests, wine, mingw, c99]

requires:
  - phase: 01-agent-foundation
    provides: aide-verify.sh, test framework conventions, stub library patterns

provides:
  - doc/protocol/acio.md — comprehensive ACIO wire protocol reference with confidence tags
  - src/test/acioemu/acioemu-pipe-test.c — 10 framing unit tests passing under Wine
  - src/test/acioemu/Module.mk — build integration for ACIO pipe tests
  - acioemu-pipe-test.exe in tests.zip and run-tests.sh

affects:
  - 02-02 (subsequent protocol test plans build on ACIO framing layer)
  - 02-03 (BIO2 tests reference ACIO framing doc)
  - phase-04 (bug investigation uses protocol doc as reference)

tech-stack:
  added: []
  patterns:
    - "Protocol-doc-first testing: document wire format → derive tests from doc → tests verify impl against doc"
    - "Stub-linked test executables: acioemu lib + hook + eamio-stub + time-stub for pipe-level tests"
    - "testexes Module.mk pattern + tests.zip manifest update for new test binaries"

key-files:
  created:
    - doc/protocol/acio.md
    - src/test/acioemu/acioemu-pipe-test.c
    - src/test/acioemu/Module.mk
  modified:
    - Module.mk (added acioemu test include and tests.zip manifest entry)
    - dist/test/run-tests.sh (added wine ./acioemu-pipe-test.exe)

key-decisions:
  - "Link acioemu pipe tests against hook lib (provides iobuf_move), eamio-stub, and time-stub to satisfy acioemu library's transitive dependencies without pulling in iohook system calls"
  - "tests.zip manifest is hardcoded in Module.mk and must be updated manually for each new test binary"
  - "config_rc.o build failure is pre-existing environmental issue (bare git repo config file shadows config/ directory); not caused by this plan"

patterns-established:
  - "Protocol reference doc format: 11-section structure with inline <!-- [Verified/Corroborated/Inferred] --> confidence tags"
  - "Pipe-level tests bypass ac_io_emu_init by calling pipe API directly (no iohook dependency in exercised paths)"

requirements-completed: [AIDE-03]

duration: 25min
completed: 2026-03-01
---

# Phase 2 Plan 1: ACIO Protocol Reference and Pipe Tests Summary

**ACIO wire protocol documented from source (11 sections, 24 confidence tags) and 10 framing unit tests pass under Wine via acioemu/pipe.c**

## Performance

- **Duration:** 25 min
- **Started:** 2026-03-01T12:42:15Z
- **Completed:** 2026-03-01T13:07:45Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Complete ACIO protocol reference at `doc/protocol/acio.md` covering wire frame format, escape encoding, checksum, autobaud, broadcast, response flag, address assignment, legacy mode, ICCA node state machine (all commands, version variants, encrypted poll cipher), and standard node commands
- 10 unit tests for the ACIO framing engine (`acioemu/pipe.c`) derived from the protocol doc, not from reading the source directly — establishing the multi-layer verification approach
- All 10 tests pass under Wine; confirmed by `make run-tests` producing "All tests successful"

## Task Commits

1. **Task 1: ACIO protocol reference document** - `ba9cda8` (docs)
2. **Task 2: ACIO pipe framing unit tests** - `422006d` (feat)

## Files Created/Modified

- `doc/protocol/acio.md` — 11-section reverse-engineered protocol reference with 24 inline confidence tags
- `src/test/acioemu/acioemu-pipe-test.c` — 10 framing tests: round-trip, SOF escape, ESCAPE escape, bad checksum, autobaud, broadcast, truncated frame recovery, response serialization, response escape serialization
- `src/test/acioemu/Module.mk` — testexes build target, links acioemu + hook + eamio-stub + time-stub + test + util
- `Module.mk` — added `include src/test/acioemu/Module.mk` and `build/bin/indep-32/acioemu-pipe-test.exe` to tests.zip manifest
- `dist/test/run-tests.sh` — added `wine ./acioemu-pipe-test.exe` as first test

## Decisions Made

- Linking against `hook` lib (not just `acioemu`) is necessary because `acioemu/pipe.c` calls `iobuf_move` which is defined in `hook/iobuf.c`. The `acioemu` static library includes `emu.c` which references `iohook_open_nul_fd`, but `--gc-sections` eliminates that symbol's transitive closure since `ac_io_emu_init` is never called from the test.
- `eamio-stub` and `time-stub` added to satisfy `acioemu/icca.c` (eamio) and `acioemu/pipe.c` (time_get_counter) transitive dependencies in the static lib.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added hook lib to test link dependencies**

- **Found during:** Task 2 (build and link)
- **Issue:** The plan specified `libs: acioemu test util` but `acioemu/pipe.c` calls `iobuf_move` from `hook/iobuf.c`. Link failed with undefined reference.
- **Fix:** Added `hook`, `eamio-stub`, `time-stub` to `libs_acioemu-pipe-test` in Module.mk
- **Files modified:** `src/test/acioemu/Module.mk`
- **Verification:** `acioemu-pipe-test.exe` links and strips successfully
- **Committed in:** `422006d` (Task 2 commit)

**2. [Rule 3 - Blocking] Added test binary to tests.zip manifest**

- **Found during:** Task 2 (build verification)
- **Issue:** `tests.zip` has a hardcoded manifest in `Module.mk`. The new test binary was not in the manifest so `make all` did not build it despite the `testexes` declaration.
- **Fix:** Added `build/bin/indep-32/acioemu-pipe-test.exe` to the `$(BUILDDIR)/tests.zip` prerequisites and `dist/test/run-tests.sh`
- **Files modified:** `Module.mk`, `dist/test/run-tests.sh`
- **Verification:** `make all --keep-going` builds `acioemu-pipe-test.exe`; `make run-tests` runs it
- **Committed in:** `422006d` (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 3 — blocking build/link issues)
**Impact on plan:** Both fixes were necessary to make the test buildable and runnable. No scope creep.

## Issues Encountered

Pre-existing `config_rc.o` build error (windres cannot find `config/usages.txt` because a bare git repository `config` file shadows the `config/` directory path). This causes `make all` to exit non-zero, and therefore `scripts/aide-verify.sh` reports `[BUILD] FAIL`. This issue was present before this plan began (confirmed by git stash + re-test). Our test builds, links, and passes — the failure is environmental and out of scope.

## Next Phase Readiness

- ACIO framing layer fully documented and tested
- Stub library pattern (eamio-stub, time-stub) established and working
- Ready for ACIO ICCA node tests (plan 02-02) and BIO2 framing tests (plan 02-03)

---
*Phase: 02-protocol-reference-and-emulator-tests*
*Completed: 2026-03-01*
