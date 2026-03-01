# Phase 2: Protocol Reference and Emulator Tests - Research

**Researched:** 2026-03-01
**Domain:** Hardware protocol reverse engineering, emulator unit testing, C99 test infrastructure
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Protocol reference doc organization**
- Hybrid structure with strict protocol family separation
- ACIO and BIO2 share framing documentation since BIO2 is ACIO-over-serial-over-USB — but
  the BIO2 doc references the shared ACIO framing section, not duplicates it
- ezusb-iidx is a completely separate protocol on a different USB device — no
  cross-contamination with ACIO/BIO2 docs
- Distinguish clearly at the device level: different USB devices, different protocols, different
  docs

**Protocol reference doc depth**
- Comprehensive reverse-engineering reference — every field, every state transition, sequencing
  rules, timing constraints, edge cases discovered from code reading
- Written as standalone markdown for both human and machine consumption
- Located at `doc/protocol/` — these are project artifacts, not planning artifacts

**Multi-layer verification approach**
- Step 1: Derive protocol documentation from existing code (RE phase, output = markdown docs)
- Step 2: Write tests derived from the docs (not from reading the code directly)
- Step 3: Cross-check — tests verify code against docs, discrepancies surface bugs in either
  layer
- The RE documentation and tests serve as independent verification layers

**Test scope and depth**
- Full state machine coverage, built incrementally: framing (pipe.c) first, then command
  dispatch (emu.c/msg.c), then full state machines (node-serial.c, node-fpga.c, security plug)
- All three protocol families: ACIO, BIO2, ezusb-iidx
- Per-module test organization following existing project conventions (one test executable per
  concern)

**Stub strategy**
- Thin stub libraries for iidxio/eamio dependencies — `src/test/stubs/` with implementations
  that return canned/configurable data
- Link test executables against stubs instead of real IO libs
- Time functions (`time_get_counter`, `time_get_elapsed_us`) also stubbed for fully deterministic
  tests
- No production code modifications — stubs are test-side only

**Ground truth and confidence levels**
- Accept the code as baseline — it's the only primary source available
- Cross-reference with external sources to strengthen confidence:
  - GitHub issue reports (actual hardware behavior vs emulator behavior)
  - Community knowledge (wiki, Discord, forums)
  - spice2x reference code (https://github.com/spice2x/spice2x.github.io/tree/main)
  - Other emulator implementations (MAME, etc.)
  - Game behavior observations (timing, polling patterns)
- Three-tier confidence tagging on each documented protocol behavior:
  - **Verified** — multiple independent sources agree
  - **Corroborated** — code matches at least one external source
  - **Inferred** — code-only, no external confirmation yet

**Bug research artifacts**
- Hypothesis-level depth: context + ranked hypotheses with evidence, but NOT full root cause
  analysis (that's Phase 4)
- Located at `.planning/reference/bugs/` — planning artifacts consumed by agents during Phase 4
- Each artifact contains: issue summary, user reports, affected games/versions, relevant code
  paths with file:line references, reproduction conditions, screenshots/logs from issue
  reporters, ranked hypotheses with supporting evidence

### Claude's Discretion
- Exact file naming within `doc/protocol/`
- Internal structure of each protocol doc (sections, ordering)
- Stub library API design (function signatures, configurability)
- Order of per-module test implementation within each protocol family
- Bug research artifact template structure

### Deferred Ideas (OUT OF SCOPE)
- Function pointer injection for IO dependencies in emulator modules — would allow swapping
  iidxio/eamio at runtime instead of compile-time stub linking
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| AIDE-03 | Reference material for hooked targets available (decompiled headers, API contracts, protocol docs) | Protocol RE from source + docs at `doc/protocol/`; bug research artifacts at `.planning/reference/bugs/`; tests validate declared behavior |
</phase_requirements>

## Summary

Phase 2 is a documentation and testing phase, not a code-change phase. The primary activities are: read emulator source, write protocol reference docs, write tests derived from those docs (not from the source directly), and produce per-bug research artifacts for three regressions. No production code is modified.

The emulator tier is well-structured for testing. The ACIO framing engine (`acioemu/pipe.c`) is pure byte manipulation with zero Win32/IO dependencies — it is the cleanest first test target. The BIO2 emulator embeds an `ac_io_emu` struct and delegates to ACIO framing, so BIO2 tests naturally build on top of ACIO pipe tests. The ezusb-iidx emulator uses a node vtable dispatch pattern (`struct ezusb_iidx_emu_node`) and depends on `iidxio.h` API calls — those calls must be stubbed out at link time for deterministic tests.

The one non-obvious structural challenge: `bio2emu-iidx/bi2a.c` calls `iidx_io_ep1_send()`, `iidx_io_ep2_recv()`, and friends directly, and `acioemu/icca.c` calls `eam_io_*` functions directly. Both link against those DLL-provided APIs. Stubs for these must export the exact function signatures from `bemanitools/iidxio.h` and `bemanitools/eamio.h` as static libraries (not DLLs), linked via `libs_` in Module.mk. The `time_get_counter` / `time_get_elapsed_us` functions in `util/time.h` also need stub variants since ACIO pipe uses them for response delay scheduling.

**Primary recommendation:** Start with `acioemu/pipe.c` tests (no stubs needed), work outward to BIO2 and ezusb-iidx nodes, produce protocol docs first and derive tests from docs second.

## Standard Stack

### Core (already in-project — no new dependencies)

| Component | Location | Purpose |
|-----------|----------|---------|
| Test framework | `src/test/test/check.h`, `check.c`, `test.h` | Assertion macros and `TEST_MODULE_BEGIN/END` |
| acioemu/pipe | `src/main/acioemu/pipe.c` + `pipe.h` | ACIO framing: serialize, deserialize, escape, checksum |
| acio message defs | `src/main/acio/acio.h` | `struct ac_io_message`, command codes, byte-order macros |
| bio2 state structs | `src/main/bio2/bi2a-iidx.h` | `struct bi2a_iidx_state_in/out` with static size assertions |
| ezusb-iidx msg defs | `src/main/ezusb-iidx/msg.h` + `*-cmd.h` | Interrupt/bulk packet formats and node IDs |
| ezusb-iidx node vtable | `src/main/ezusb-emu/node.h` | `struct ezusb_iidx_emu_node` interface |

### Stub Libraries to Create

| Stub | Covers | Signatures from |
|------|--------|----------------|
| `iidxio-stub` | `iidxio.h` API | `src/main/bemanitools/iidxio.h` |
| `eamio-stub` | `eamio.h` API | `src/main/bemanitools/eamio.h` |
| `time-stub` | `time_get_counter`, `time_get_elapsed_us`, etc. | `src/main/util/time.h` |

These are static `.a` libraries placed at `src/test/stubs/` with their own `Module.mk`. Stub functions return safe defaults (zero, true, empty buffers) and optionally accept configuration via module-level globals set before each test.

## Architecture Patterns

### Test Module Structure (established pattern)

One test binary per concern. Module.mk declares it with `testexes +=`. Library dependencies listed under `libs_`. Source files under `src_`. Test dir under `srcdir_`.

```makefile
testexes            += acioemu-pipe-test

srcdir_acioemu-pipe-test := src/test/acioemu

libs_acioemu-pipe-test  := \
    acioemu \
    test \
    util \

src_acioemu-pipe-test   := \
    acioemu-pipe-test.c \
```

The new test Module.mk files are included in `Module.mk` at the root of the project at lines 208-214 alongside the existing test includes. A new `include src/test/acioemu/Module.mk` line goes there.

### Test File Pattern (established)

```c
#include "acioemu/pipe.h"
#include "test/check.h"
#include "test/test.h"

static void test_framing_round_trip()
{
    struct ac_io_out out;
    /* ... setup, exercise, assert ... */
    check_data_eq(result, result_len, expected, expected_len);
}

TEST_MODULE_BEGIN("acioemu-pipe")
TEST_MODULE_TEST(test_framing_round_trip)
TEST_MODULE_END()
```

Failure calls `abort()` via `check_*_failed` — each test executable exits non-zero on any failure.

### Stub Library Pattern (new, established in this phase)

Thin static lib at `src/test/stubs/{name}/`. Exports the exact symbol names the real lib exports. Module.mk declares it as a `libs +=` (not `testexes`), so it compiles into a `.a` linkable by test executables.

```c
/* src/test/stubs/iidxio/iidxio-stub.c */
#include "bemanitools/iidxio.h"

static uint16_t stub_keys = 0;

void iidxio_stub_set_keys(uint16_t keys) { stub_keys = keys; }

bool iidx_io_ep2_recv(void) { return true; }
uint16_t iidx_io_ep2_get_keys(void) { return stub_keys; }
uint8_t iidx_io_ep2_get_turntable(uint8_t player_no) { return 0; }
/* ... all other iidxio.h functions ... */
```

Stub lib declared in Module.mk as a regular `libs` entry:

```makefile
libs            += iidxio-stub
srcdir_iidxio-stub := src/test/stubs/iidxio
src_iidxio-stub    := iidxio-stub.c
```

Test executables that need it add `iidxio-stub` to their `libs_` list instead of any real iidxio lib.

### Protocol Doc Structure (doc/protocol/)

Three files planned (exact names at Claude's discretion):

- `doc/protocol/acio.md` — ACIO framing layer: SOF (0xAA), ESCAPE (0xFF), checksum, message struct layout (`struct ac_io_message`), broadcast vs command address space, response flag (0x80), address assignment sequence, legacy mode behavior, ICCA node state machine
- `doc/protocol/bio2.md` — BIO2-over-ACIO: references `acio.md` for framing, adds BIO2-specific addressing (single node at addr 1), BI2A command set (INIT, WATCHDOG, POLL, POLL_WD, GET_VERSION, START_UP, KEEPALIVE), poll state structs (`bi2a_iidx_state_in/out`), poll limiter (Sleep(1)), turntable accumulator, coin latch logic
- `doc/protocol/ezusb-iidx.md` — ezusb-iidx: separate USB device (not ACIO), interrupt pipe (write: light control + node command; read: pad state + seq_no + status), bulk pipe (read/write: 64-byte page packets), node dispatch table, all node IDs and their command sets (FPGA_V1, FPGA_V2, SERIAL, SECURITY_PLUG, EEPROM, SRAM, COIN, WDT, 16SEG), board type differences (C02 vs D01), serial sub-protocol (H8 messages, card reader state machine)

Confidence tags per-behavior inline in each doc:
- `<!-- [Verified] -->` — code + external source agree
- `<!-- [Corroborated] -->` — code + one external source
- `<!-- [Inferred] -->` — code only

### Bug Research Artifact Structure

Located at `.planning/reference/bugs/`. One markdown file per bug:

```
.planning/reference/bugs/
├── 345-popnmusic-15-18-regression.md
├── 344-iidx-11-15-song-timing.md
└── 351-iidx-tricoro-cn-boot.md
```

Each contains:
1. Issue summary (one paragraph)
2. User reports summary (OS, hardware, game version, symptoms)
3. Affected games/versions
4. Relevant code paths with `file:line` references
5. Reproduction conditions known from reports
6. Ranked hypotheses with supporting evidence from code reading

## Known Code Facts (from source reading)

### ACIO Protocol (HIGH confidence from source)

**Wire framing:**
- SOF byte: `0xAA` (`AC_IO_SOF`)
- Escape byte: `0xFF` (`AC_IO_ESCAPE`) — escapes SOF and ESCAPE by writing `ESCAPE, ~byte`
- Message layout: `[SOF] [addr] [code:2] [seq_no] [nbytes] [payload:nbytes] [checksum]`
- Checksum: 8-bit sum of all bytes from addr through end of payload (not including checksum itself)
- Broadcast address: `0x70` (`AC_IO_BROADCAST`) — different struct layout (no code/seq_no)
- Response flag: OR with `0x80` on addr byte for responses
- Autobaud frame: SOF followed immediately by another SOF — yields NULL message pointer from `ac_io_out_get_message()`

**Legacy mode:** Enabled via `ac_io_legacy_mode()` — drains only one response per cycle. Required for popn 15-18 slotted readers (old libacio that drops multi-message buffers).

**Address assignment sequence:** Host sends to addr 0, cmd `AC_IO_CMD_ASSIGN_ADDRS` (0x0001). Response at addr 0 with node count. Subsequent commands go to addr 1..N.

**ICCA node commands** (from `acioemu/icca.c`):
- `AC_IO_CMD_GET_VERSION` → responds with `struct ac_io_version`; node type `0x03000000`; versions 1.5.0, 1.6.0, 1.7.0
- `AC_IO_CMD_START_UP` → status 0x00
- `AC_IO_ICCA_CMD_QUEUE_LOOP_START` → clears fault, begins polling
- `AC_IO_ICCA_CMD_SET_SLOT_STATE` → maps to `eam_io_card_slot_cmd()`
- `AC_IO_ICCA_CMD_POLL` / `AC_IO_ICCA_CMD_POLL_ENCRYPTED` → returns `struct ac_io_icca_state`; poll delay emulated (max 16ms)
- `AC_IO_ICCA_CMD_POLL_FELICA` → version-dependent behavior
- `AC_IO_ICCA_CMD_KEY_EXCHANGE` → Marsaglia KISS key setup, reader key `0x14243444`

**Encrypted poll** (v160+): CRC16-MSB over state bytes, then XOR cipher (modified KISS PRNG).

### BIO2 Protocol (HIGH confidence from source)

- BIO2 is ACIO-over-USB-serial: same framing, single node at bus address 1
- Dispatcher in `bio2emu/emu.c`: addr 0 → `ac_io_emu_cmd_assign_addrs(..., 1)`, addr 1 → game-specific dispatcher
- BI2A (IIDX) commands: INIT (0x?), WATCHDOG, POLL, POLL_WD, plus standard ACIO GET_VERSION/START_UP/KEEPALIVE
- Poll limiter: `Sleep(1)` called inside `bio2_emu_bi2a_send_state()` unless `disable_poll_limiter=true` — must be disabled in tests
- Poll state: turntable accumulator for TT multiplier mode, coin latch (edge-detect on sys COIN bit), slider defaults from `vefx.txt`
- `bi2a_iidx_state_out` size: 48 bytes (static assert). `bi2a_iidx_state_in` size: 46 bytes (static assert)

### ezusb-iidx Protocol (HIGH confidence from source)

**USB pipes:**
- Interrupt OUT → `struct ezusb_iidx_msg_interrupt_write_packet`: deck_lights (u16), panel_lights (u8), node (u8), cmd (u8), cmd_detail[2] (u8), top_lamps (u8), fpga_run (u8), etc.
- Interrupt IN → `struct ezusb_iidx_msg_interrupt_read_packet`: inverted_pad (u32), status (u8), turntables (u8x2), seq_no (u8), fpga_write_ready (u8), serial_io_busy_flag (u8), sliders (u8x3)
- Bulk packets: `struct ezusb_iidx_msg_bulk_packet` = node (u8) + page (u8) + payload[62]

**inverted_pad bit layout** (bits are active-low, so inverted before use):
- bits 8-14: P1 keys 1-7
- bits 15-21: P2 keys 1-7
- bits 24-27: panel (start1, start2, vefx, effector)
- bits 28-30: sys (test, service, coin)
- bit 22: coin mode from coin node
- bit 4: board type (1=C02, 0=D01 — active low)
- bit 31: coin mode state

**Node dispatch:** `ezusb_iidx_emu_msg_nodes[256]` table indexed by node ID. Interrupt write sets `cur_node`; next bulk read dispatches to `cur_node->read_packet()`.

**FPGA V1 init sequence** (iidx 9-13): INIT → CHECK (0xFF) → CHECK_2 (0x02) → WRITE (0x03) bursts → WRITE_DONE (0x04). Status codes: OK=0x00, confusingly OK_2=FAULT=0xFE.

**FPGA V2** (iidx 14+): Same sequence but cleaner status codes: INIT_OK=0x41, CHECK_OK=0x42, WRITE_OK=0x43, FAULT=0xFE. `fpga2_check_flag_unkn` in interrupt read must be 2.

**Serial node** (V1 only, iidx 9-13): Sub-protocol layered on top of bulk pipe. H8 framing (CMD_H8_REQ=0xAA, CMD_H8_RESP=0xA5) wraps node commands. Magnetic card state machine: INIT → GET_STATUS → OPEN_SLOT → (card insert) → READ → GET_STATUS cycle. The `.c` include of `card-mag.c` means tests compile both files together.

**Board type difference:** D01 forces bit 4 of inverted_pad to 0 (active-low → board type pin not set). C02 leaves it alone.

## Common Pitfalls

### Pitfall 1: Testing Against Current Emulator Output Instead of Declared Behavior

**What goes wrong:** Test asserts `result == whatever_the_code_currently_produces` rather than `result == what_the_protocol_requires`. When the emulator has a bug, the test passes incorrectly.

**How to avoid:** Write tests against documented protocol behavior first. Document first, test second — the doc is the source of truth. If a test fails after writing it correctly, it means either the doc is wrong or the emulator is wrong — investigate which.

### Pitfall 2: Forgetting the Time Stub

**What goes wrong:** `ac_io_in_drain()` calls `time_get_elapsed_us()` to check response delays. Without a stub, tests calling drain will use real `QueryPerformanceCounter`-based time — which works but makes tests non-deterministic and hard to reason about for delay-behavior tests.

**How to avoid:** Create a time stub that returns a controllable counter value. For basic pipe tests that don't need to test timing, a stub that always returns a large elapsed value (e.g., 1,000,000 us) forces all queued responses to be immediately available.

### Pitfall 3: node-serial.c Includes card-mag.c Directly

**What goes wrong:** `node-serial.c` has `#include "ezusb-iidx-emu/card-mag.c"` at the top — it includes a `.c` file. Test executables for node-serial must NOT also list `card-mag.c` in their `src_` list, or they'll get duplicate symbols.

**How to avoid:** Only list `node-serial.c` in `src_`; `card-mag.c` is compiled as part of `node-serial.c`.

### Pitfall 4: bio2emu-iidx Sleep(1) in Tests

**What goes wrong:** `bio2_emu_bi2a_send_state()` calls `Sleep(1)` when `poll_delay` is true. If tests call the BIO2 poll dispatch without disabling the limiter, each test iteration sleeps 1ms, making the full test suite slow.

**How to avoid:** Call `bio2_emu_bi2a_init(..., true)` (second arg = `disable_poll_limiter`) in test setup. The poll limiter exists only to throttle game polling; tests don't need it.

### Pitfall 5: eamio/iidxio Linkage Errors

**What goes wrong:** `acioemu/icca.c` calls `eam_io_poll()`, `eam_io_get_keypad_state()`, etc. Without a stub, linking an `acioemu-icca-test` against `acioemu` + `test` + `util` will produce undefined symbol errors at link time.

**How to avoid:** Always add `eamio-stub` (or `iidxio-stub`) to `libs_` when testing modules that link against those APIs. The stubs provide default-safe implementations.

### Pitfall 6: iohook_open_nul_fd in Tests

**What goes wrong:** `ac_io_emu_init()` calls `iohook_open_nul_fd()` which requires the iohook subsystem to be initialized. Tests that construct `struct ac_io_emu` directly will fail at that call.

**How to avoid:** For pipe-level tests, test `acioemu/pipe.c` functions directly (`ac_io_out_init`, `ac_io_out_supply`, etc.) without going through `ac_io_emu_init`. The pipe layer has no Win32/IO dependencies. Emu-level tests that need full `ac_io_emu` initialization require a hook stub or avoidance of `ac_io_emu_init`.

### Pitfall 7: Module.mk Include Missing

**What goes wrong:** New test Module.mk files won't be picked up unless added to the root `Module.mk`. The build succeeds but the new tests aren't built or run.

**How to avoid:** Add `include src/test/{module}/Module.mk` to the root `Module.mk` at the test include block (lines 208-214).

## Test Plan by Module

### ACIO Tier

**`src/test/acioemu/` — `acioemu-pipe-test`**
- Libraries: `acioemu`, `test`, `util` (no stubs needed — pipe.c has no iohook/time deps in the main exercised paths, though time stub recommended for determinism)
- Tests:
  - Round-trip: supply escaped bytes → `ac_io_out_supply` → `ac_io_out_get_message` returns correct `ac_io_message`
  - SOF escape: byte 0xAA in payload is escaped as `[0xFF, 0x55]`
  - ESCAPE escape: byte 0xFF in payload is escaped as `[0xFF, 0x00]`
  - Checksum validation: bad checksum → message rejected, `have_message` stays false
  - Autobaud frame: SOF immediately followed by SOF → `get_message` returns NULL
  - Broadcast message: addr=0x70 yields broadcast struct layout
  - Truncated frame: SOF mid-frame resets state
  - Response serialization: `ac_io_in_supply` with message → drain produces correctly escaped/checksummed bytes
  - Legacy mode: two responses in queue → drain only emits one

**`acioemu-icca-test` (separate binary)**
- Libraries: `acioemu`, `eamio-stub`, `test`, `util`, plus time-stub if testing delay behavior
- Requires eamio-stub for `eam_io_poll`, `eam_io_get_keypad_state`, `eam_io_get_sensor_state`, `eam_io_read_card`, `eam_io_card_slot_cmd`
- Cannot test via `ac_io_emu_init` (iohook dependency) — test icca dispatch logic by constructing `struct ac_io_emu_icca` with a fake `struct ac_io_emu` (zero-initialized, no fd), calling `ac_io_emu_icca_dispatch_request` directly
- Tests:
  - Version response: GET_VERSION → response has type=ICCA (0x03000000), version matches configured enum
  - Startup sequence: START_UP → status 0x00; fault is cleared; polling not started
  - Queue loop start: QUEUE_LOOP_START → fault cleared, polling_started=true
  - Poll before queue loop: POLL → status_code=FAULT (polling hasn't started guard)
  - Poll after queue loop: POLL → status_code=IDLE
  - Card insert: sensor stub reports both sensors → card_result=EAM_IO_CARD_ISO15693 → status_code=GOT_UID, uid matches stub
  - Keypad event: keypad stub reports button press → key_events populated
  - Felica version branches: v150/v160/v170 each behave differently on POLL_FELICA
  - Encrypted poll: response has CRC16 appended, bytes XOR'd via KISS cipher

### BIO2 Tier

**`src/test/bio2emu/` — `bio2emu-iidx-test`**
- Libraries: `bio2emu`, `bio2emu-iidx`, `acioemu`, `iidxio-stub`, `test`, `util`
- Avoid calling `bio2emu_port_init` (calls `iohook_open_nul_fd` and `rs232_hook_add_fd`) — test `bio2_emu_bi2a_dispatch_request` directly with a constructed `struct bio2emu_port`
- Tests:
  - GET_VERSION → response node type=BI2A (0x0D060000)
  - START_UP → status 0x00
  - INIT → status 0x00
  - WATCHDOG → status 0x00
  - KEEPALIVE → empty response (nbytes=0)
  - POLL with known iidxio-stub state → response body matches expected packed bit fields
  - POLL: coin latch logic — coin bit on → count increments once; coin bit held → count does not re-increment
  - POLL: TT multiplier mode — stub returns delta, response TURNTABLE1 reflects multiplied value
  - Address assignment: addr=0 cmd=ASSIGN_ADDRS → response count=1

### ezusb-iidx Tier

**`src/test/ezusb-iidx-emu/` — multiple binaries**

**`ezusb-iidx-emu-fpga-test`**
- Libraries: `ezusb-iidx-emu`, `ezusb-emu`, `iidxio-stub`, `test`, `util`
- Tests fpga_v1 and fpga_v2 node command handling
- FPGA V1: INIT → OK; CHECK(0xFF) → OK; CHECK_2(0x02) → status 2; WRITE → OK; WRITE_DONE → OK
- FPGA V2: INIT → INIT_OK (0x41); CHECK → CHECK_OK (0x42); WRITE → WRITE_OK (0x43)
- `fpga2_check_flag_unkn` field in interrupt read packet = 2

**`ezusb-iidx-emu-msg-test`**
- Libraries: `ezusb-iidx-emu`, `ezusb-emu`, `iidxio-stub`, `test`, `util`
- Tests interrupt read/write and bulk dispatch
- Interrupt write → node remembered as `cur_node`; process_cmd called; bulk read dispatches to that node
- Interrupt read: keys mapped to inverted_pad correctly (P1 keys at bits 8-14, active low)
- Board type C02 vs D01: inverted_pad bit 4 difference

**`ezusb-iidx-emu-serial-test`** (if serial sub-protocol can be isolated)
- Requires eamio-stub for card reader API
- Tests H8 framing, NODE_ENUM sequence, card read/write state machine
- Note: `node-serial.c` includes `card-mag.c` via `#include` — only list `node-serial.c` in src_

## Bug Research Artifacts

Three artifacts to produce at `.planning/reference/bugs/`:

**`345-popnmusic-15-18-regression.md`**
- Pop'n music 15-18 boots/runs broken; v5.43→v5.44 regression (also related: #341, #338)
- Relevant code: `popnhook1/dllmain.c`, `popnhook-util/acio.c`, `acioemu/icca.c`, `acioemu/pipe.c`
- `popnhook_acio_init(true)` enables legacy mode — a regression here could change ACIO framing behavior for slotted readers
- `ezusb2-popn-emu` also involved for the USB2 side
- Key hypothesis to investigate: change to `acioemu/pipe.c` legacy mode logic or `icca.c` state machine between 5.43 and 5.44

**`344-iidx-11-15-song-timing.md`**
- IIDX 11-15 song selection timing broken on modern hardware
- Relevant code: `ezusb-iidx-emu/msg.c` (interrupt read/write timing), `util/time.c` (counter precision)
- These games use `iidxhook1`-`iidxhook5` + ezusb emulator, no BIO2
- Hypothesis: timing-dependent behavior in interrupt polling loop sensitive to counter resolution

**`351-iidx-tricoro-cn-boot.md`**
- IIDX tricoro fails to boot with CN network config
- Relevant code: `iidxhook9/reverbfix.c` (CoCreateInstance hook for DSFX_STANDARD_I3DL2REVERB), `iidxhook-util/`, CN-specific network code
- iidxhook9 is the tricoro hook; CN variant means different network/security config
- reverbfix is already implemented; boot failure may relate to security, network, or eamuse config

## Code Examples

### ACIO Framing Round-Trip Test Sketch

```c
/* Source: acioemu/pipe.c analysis */
static void test_acio_framing_command()
{
    struct ac_io_out out;
    uint8_t wire_bytes[] = {
        AC_IO_SOF,       /* frame start */
        0x01,            /* addr=1 */
        0x00, 0x02,      /* code=GET_VERSION (big-endian) */
        0x00,            /* seq_no */
        0x00,            /* nbytes=0 */
        0x03             /* checksum: 0x01+0x00+0x02+0x00+0x00 = 0x03 */
    };
    struct const_iobuf src = {wire_bytes, sizeof(wire_bytes), 0};

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_true(ac_io_out_have_message(&out));

    const struct ac_io_message *msg = ac_io_out_get_message(&out);
    check_non_null(msg);
    check_int_eq(msg->addr, 0x01);
    check_int_eq(ac_io_u16(msg->cmd.code), AC_IO_CMD_GET_VERSION);
    check_int_eq(msg->cmd.seq_no, 0x00);
    check_int_eq(msg->cmd.nbytes, 0x00);
}
```

### iidxio Stub Skeleton

```c
/* Source: bemanitools/iidxio.h — must match all exported symbols */
#include "bemanitools/iidxio.h"
#include <stdbool.h>
#include <stdint.h>

/* Configurable state for tests */
static uint16_t stub_keys;
static uint8_t stub_panel;
static uint8_t stub_sys;
static uint8_t stub_turntable[2];
static uint8_t stub_slider[5];

void iidxio_stub_set_keys(uint16_t k) { stub_keys = k; }
void iidxio_stub_set_sys(uint8_t s) { stub_sys = s; }

/* Required iidxio.h exports */
void iidx_io_set_loggers(log_formatter_t m, log_formatter_t i,
    log_formatter_t w, log_formatter_t f) {}
bool iidx_io_init(thread_create_t c, thread_join_t j,
    thread_destroy_t d) { return true; }
void iidx_io_fini(void) {}
void iidx_io_ep1_set_deck_lights(uint16_t v) {}
void iidx_io_ep1_set_panel_lights(uint8_t v) {}
void iidx_io_ep1_set_top_lamps(uint8_t v) {}
void iidx_io_ep1_set_top_neons(bool v) {}
bool iidx_io_ep1_send(void) { return true; }
bool iidx_io_ep2_recv(void) { return true; }
uint8_t iidx_io_ep2_get_turntable(uint8_t p) { return stub_turntable[p]; }
uint8_t iidx_io_ep2_get_slider(uint8_t n) { return stub_slider[n]; }
uint8_t iidx_io_ep2_get_sys(void) { return stub_sys; }
uint8_t iidx_io_ep2_get_panel(void) { return stub_panel; }
uint16_t iidx_io_ep2_get_keys(void) { return stub_keys; }
bool iidx_io_ep3_write_16seg(const char *t) { return true; }
```

### Protocol Doc Section Example (ACIO)

```markdown
## Message Frame Format

<!-- [Verified] — matches pipe.c serialization and game-side libacio behavior -->

Every ACIO message on the wire is wrapped in a frame:

| Byte(s) | Field | Notes |
|---------|-------|-------|
| 0xAA | SOF (Start of Frame) | Always literal 0xAA; not escaped |
| 1 | addr | High bit set = response; bit 7 clear = request |
| 2-3 | code | Big-endian u16 command code |
| 4 | seq_no | Sequence number, echoed in response |
| 5 | nbytes | Payload length in bytes |
| 6..5+nbytes | payload | Command-specific data |
| 6+nbytes | checksum | 8-bit sum of all bytes from addr through end of payload |

**Escape encoding** <!-- [Verified] -->: Any byte value 0xAA or 0xFF in the
payload is escaped as `[0xFF, ~byte]`. SOF is never escaped in the header
position (first byte). The escape sequence for 0xAA is `[0xFF, 0x55]`; for
0xFF is `[0xFF, 0x00]`.
```

## Validation Architecture

No `nyquist_validation` workflow setting detected in `.planning/config.json` — skipping this section.

Tests run via `aide-verify.sh` which calls `make run-tests`. All test binaries are built as part of `make build` and run under Wine. A new test binary failing under Wine fails the `[TEST]` stage of `aide-verify.sh`.

## Open Questions

1. **Can `ac_io_emu_init` be avoided in ACIO emu-level tests?**
   - What we know: `ac_io_emu_init` calls `iohook_open_nul_fd` which requires iohook initialization
   - What's unclear: Whether a minimal iohook stub is feasible, or whether the dispatch-layer tests must be structured to bypass `ac_io_emu_init` entirely
   - Recommendation: Test pipe and icca dispatch in isolation without full emu init; add a `iohook-stub` to `src/test/stubs/` if emu-level dispatch tests become needed

2. **spice2x cross-reference depth**
   - What we know: spice2x is available at https://github.com/spice2x/spice2x.github.io as a reference for expected protocol behavior
   - What's unclear: How much of the emulator behavior can be directly corroborated vs remains inferred
   - Recommendation: Use spice2x to upgrade "Inferred" → "Corroborated" tags in protocol docs where their code matches; focus on timing-sensitive behaviors (poll delays, coin counting) which are most likely to have hardware-driven constraints

3. **eamio.h stub completeness for icca tests**
   - What we know: `acioemu/icca.c` calls `eam_io_poll`, `eam_io_get_keypad_state`, `eam_io_get_sensor_state`, `eam_io_read_card`, `eam_io_card_slot_cmd`
   - What's unclear: Full eamio.h function set — need to verify complete list before writing stub
   - Recommendation: Read `src/main/bemanitools/eamio.h` fully before writing stub; stub all exported functions

## Sources

### Primary (HIGH confidence)
- `src/main/acioemu/pipe.c` + `pipe.h` — ACIO framing implementation, ground truth for wire format
- `src/main/acioemu/emu.c` + `emu.h` — ACIO emulator IRP dispatch
- `src/main/acioemu/icca.c` + `icca.h` — ICCA card reader emulator, all node commands
- `src/main/acioemu/addr.c` + `addr.h` — Address assignment logic
- `src/main/acio/acio.h` — Complete message struct and constants
- `src/main/bio2emu/emu.c` + `emu.h` — BIO2 emulator structure
- `src/main/bio2emu-iidx/bi2a.c` + `bi2a.h` — BI2A IIDX command dispatcher
- `src/main/bio2/bi2a-iidx.h` — BIO2 IIDX poll state structs with size assertions
- `src/main/ezusb-iidx-emu/msg.c` — ezusb-iidx interrupt/bulk dispatch
- `src/main/ezusb-iidx-emu/node-serial.h` — Serial node interface
- `src/main/ezusb-iidx-emu/node-fpga.h` — FPGA node interface
- `src/main/ezusb-iidx/msg.h` — Interrupt/bulk packet struct definitions
- `src/main/ezusb-iidx/fpga-cmd.h` — FPGA command and status codes
- `src/main/ezusb-iidx/serial-cmd.h` — Serial command codes
- `src/main/ezusb-emu/node.h` — Node vtable interface
- `src/test/test/check.h` + `test.h` — Test framework macros
- `src/test/security/Module.mk` — Canonical test Module.mk pattern
- `src/test/security/security-id-test.c` — Canonical test file pattern
- `Module.mk` lines 208-214 — Where test Module.mk files are included

### Secondary (MEDIUM confidence)
- `src/main/popnhook-util/acio.c` — Confirms legacy mode usage for popn 15-18 (corroborates REGR-01 hypothesis)
- `src/main/popnhook1/dllmain.c` — popnhook1 initialization sequence
- `src/main/iidxhook9/reverbfix.c` — tricoro CN reverb fix (context for REGR-03)
- `.planning/codebase/TESTING.md` — Existing documentation of test conventions
- `.planning/codebase/ARCHITECTURE.md` — Protocol layer architecture map

### Tertiary (LOW confidence — to be elevated via doc research)
- spice2x reference implementation — for "Corroborated" confidence tagging (not yet examined)
- GitHub issues #345, #344, #351 — user reports to inform bug artifact hypotheses (not examined in source code search)

## Metadata

**Confidence breakdown:**
- Protocol facts from source: HIGH — read directly from implementation files
- Test infrastructure patterns: HIGH — established patterns in existing test modules
- Bug hypotheses: LOW-MEDIUM — requires issue content and git diff analysis to elevate
- spice2x cross-references: LOW — not examined yet, available for confidence elevation

**Research date:** 2026-03-01
**Valid until:** 2026-06-01 (stable codebase; no fast-moving external deps)
