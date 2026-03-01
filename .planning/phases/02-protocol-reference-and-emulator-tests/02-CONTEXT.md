# Phase 2: Protocol Reference and Emulator Tests - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Give agents ground-truth protocol reference material and automated correctness signals for the hardware emulator tier. Covers three protocol families (ACIO, BIO2, ezusb-iidx), emulator unit tests across all layers, and per-bug research artifacts for regressions #345, #344, #351.

Does NOT include: fixing the regressions (Phase 4), Ghidra RE integration (Phase 3), or modifying production emulator code.

</domain>

<decisions>
## Implementation Decisions

### Protocol reference doc organization
- Hybrid structure with strict protocol family separation
- ACIO and BIO2 share framing documentation since BIO2 is ACIO-over-serial-over-USB — but the BIO2 doc references the shared ACIO framing section, not duplicates it
- ezusb-iidx is a completely separate protocol on a different USB device — no cross-contamination with ACIO/BIO2 docs
- Distinguish clearly at the device level: different USB devices, different protocols, different docs

### Protocol reference doc depth
- Comprehensive reverse-engineering reference — every field, every state transition, sequencing rules, timing constraints, edge cases discovered from code reading
- Written as standalone markdown for both human and machine consumption
- Located at `doc/protocol/` — these are project artifacts, not planning artifacts

### Multi-layer verification approach
- Step 1: Derive protocol documentation from existing code (RE phase, output = markdown docs)
- Step 2: Write tests derived from the docs (not from reading the code directly)
- Step 3: Cross-check — tests verify code against docs, discrepancies surface bugs in either layer
- The RE documentation and tests serve as independent verification layers. If a test fails, the question is "is the code wrong or is the doc wrong?" — having both as separate artifacts makes that answerable

### Test scope and depth
- Full state machine coverage, built incrementally: framing (pipe.c) first, then command dispatch (emu.c/msg.c), then full state machines (node-serial.c, node-fpga.c, security plug)
- All three protocol families: ACIO, BIO2, ezusb-iidx
- Per-module test organization following existing project conventions (one test executable per concern)

### Stub strategy
- Thin stub libraries for iidxio/eamio dependencies — `src/test/stubs/` with implementations that return canned/configurable data
- Link test executables against stubs instead of real IO libs
- Time functions (`time_get_counter`, `time_get_elapsed_us`) also stubbed for fully deterministic tests
- No production code modifications — stubs are test-side only

### Ground truth and confidence levels
- Accept the code as baseline — it's the only primary source available
- Cross-reference with external sources to strengthen confidence:
  - GitHub issue reports (actual hardware behavior vs emulator behavior)
  - Community knowledge (wiki, Discord, forums)
  - spice2x reference code (https://github.com/spice2x/spice2x.github.io/tree/main) — valuable for expected behavior and protocol insights
  - Other emulator implementations (MAME, etc.)
  - Game behavior observations (timing, polling patterns)
- Three-tier confidence tagging on each documented protocol behavior:
  - **Verified** — multiple independent sources agree (code + issues + spice2x + game behavior)
  - **Corroborated** — code matches at least one external source
  - **Inferred** — code-only, no external confirmation yet
- "Inferred" items are highest-value targets for future verification (Phase 3 Ghidra RE, hardware testing)

### Bug research artifacts
- Hypothesis-level depth: context + ranked hypotheses with evidence, but NOT full root cause analysis (that's Phase 4)
- Located at `.planning/reference/bugs/` — planning artifacts consumed by agents during Phase 4
- Each artifact contains: issue summary, user reports, affected games/versions, relevant code paths with file:line references, reproduction conditions, screenshots/system specs/log outputs from issue reporters, ranked hypotheses with supporting evidence

### Claude's Discretion
- Exact file naming within `doc/protocol/` (e.g. `acio.md` vs `acio-protocol.md`)
- Internal structure of each protocol doc (sections, ordering)
- Stub library API design (function signatures, configurability)
- Order of per-module test implementation within each protocol family
- Bug research artifact template structure

</decisions>

<specifics>
## Specific Ideas

- bemanitools' core principle is Win32 API-level emulation at the lowest level possible. This ensures highest emulation quality for running games faithfully without real hardware, or on real hardware with partial emulation for compatibility and preservation. This principle must never be broken.
- spice2x has similar goals but different technical approach (higher-level hooks, not full device emulation at Win32 API level). Use it as behavioral reference and cross-check source, not as architectural guidance. Stick to bemanitools principles for all solution approaches.
- Protocol docs should be the kind of reference that lets an agent write a test without looking at the emulator source — the doc alone should be sufficient to know "given this input, expect this output."

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `acioemu/pipe.c`: Pure byte-manipulation framing engine (serialize/deserialize/escape/checksum). Zero Win32/IO dependencies. Cleanest first test target.
- `check.h` / `test.h`: Existing test framework with `TEST_MODULE_BEGIN/END`, `check_int_eq`, `check_str_eq`, `check_data_eq` (byte buffer comparison), etc. Each test is a standalone executable.
- `acio/acio.h`: Complete message format definition with packed structs, command codes, byte-order macros. Primary input for ACIO protocol RE doc.
- `bio2/bi2a-iidx.h`: Poll state structs with static size assertions. Primary input for BIO2 RE doc.
- `ezusb-iidx/msg.h` + per-node command headers: Packet formats and node IDs. Primary input for ezusb-iidx RE doc.

### Established Patterns
- Test executables: one binary per concern, linked against `test` lib (check.c) and `util` (log.h). `Module.mk` defines `testexes +=`, `srcdir_`, `libs_`, `src_`.
- No mock/stub pattern exists yet — this phase establishes the first one.
- `node.h` vtable pattern in ezusb-emu: `{ node_id, init_node, process_cmd, read_packet, write_packet }`. Every addressable node implements this interface.

### Integration Points
- Test stubs must match function signatures from `bemanitools/iidxio.h` and `bemanitools/eamio.h`
- `bio2emu_port` embeds `ac_io_emu` — BIO2 tests depend on ACIO framing working first
- `node-serial.c` includes `card-mag.c` via `#include` of .c file — tests would compile both together
- `bi2a.c` has a `Sleep(1)` poll limiter that needs bypass (`disable_poll_limiter=true`) in test context

</code_context>

<deferred>
## Deferred Ideas

- Function pointer injection for IO dependencies in emulator modules — would allow swapping iidxio/eamio at runtime instead of compile-time stub linking. Increases flexibility and reusability of test infrastructure. Note as future improvement, do not modify production code in this phase.

</deferred>

---

*Phase: 02-protocol-reference-and-emulator-tests*
*Context gathered: 2026-03-01*
