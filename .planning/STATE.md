---
gsd_state_version: 1.0
milestone: v5.43
milestone_name: milestone
status: unknown
last_updated: "2026-03-01T13:26:12.010Z"
progress:
  total_phases: 2
  completed_phases: 2
  total_plans: 7
  completed_plans: 7
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-28)

**Core value:** Existing supported games must boot and run correctly on current hardware — regressions are the highest priority.
**Current focus:** Phase 3 — Ghidra Integration (decompilation tooling)

## Current Position

Phase: 2 of 6 (Protocol Reference and Emulator Tests) — COMPLETE
Plan: 5 of 5 complete (02-05 ICCA node tests + bug research artifacts)
Status: Phase 2 complete — all protocol docs, emulator tests, and bug research artifacts done
Last activity: 2026-03-01 — Phase 2 plan 05 (ICCA tests and bug research) completed

Progress: [█████░░░░░] 50%

## Performance Metrics

**Velocity:**
- Total plans completed: 2
- Average duration: ~2 min
- Total execution time: ~4 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-agent-foundation | 2 | ~4 min | ~2 min |

**Recent Trend:**
- Last 5 plans: 01-01 (~2 min), 01-02 (~2 min)
- Trend: Fast documentation plans

*Updated after each plan completion*
| Phase 01 P01 | 3 | 2 tasks | 3 files |
| Phase 02 P02 | 4 | 2 tasks | 10 files |
| Phase 02-protocol-reference-and-emulator-tests P03 | 11 | 2 tasks | 3 files |
| Phase 02 P04 | 12 | 2 tasks | 5 files |
| Phase 02 P05 | 13 | 2 tasks | 7 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Init]: Fix bugs on BT5, not BT6 — BT6 is WIP, users need fixes now
- [Init]: Triage all issues before deep-diving
- [Init]: AIDE infrastructure is top priority — enables agentic bug investigation
- [01-01]: aide-verify.sh uses git diff --exit-code after make code-format for format checking (avoids clang-format version dependencies)
- [01-01]: Native runner CI step for Wine tests (keeps Dockerfile.build unchanged; avoids Wine version lock-in in Docker image)
- [01-02]: agent-reference.md links out to existing docs rather than duplicating — keeps it concise and maintainable
- [01-02]: CLAUDE.md is self-contained (no dependency on personal ~/.claude/rules/) — works for any contributor
- [01-02]: Tier 2 includes modules with tests (d3d9hook, iidxhook8) alongside untested emulation — both are hardware-free and Wine-testable
- [Phase 02]: Test stubs placed in src/test/stubs/ as static libs; included via stubs/X/X-stub.h path resolved by -I src/test
- [Phase 02]: time-stub defaults to 1s elapsed to make emulator drain loops immediately available without timing configuration
- [02-01]: Link pipe-level tests against hook lib (iobuf_move) + eamio-stub + time-stub; tests.zip manifest is hardcoded and must be updated manually for each new test binary
- [02-01]: Protocol doc established before tests — doc is source of truth; discrepancies between doc and impl surface bugs in either layer
- [Phase 02-03]: Bypass bio2_emu_bi2a_init by zero-initializing port struct and calling ac_io_in/out_init directly — tests dispatch without iohook
- [Phase 02-03]: hooklib required in test link deps for rs232_hook_add_fd (referenced in bio2emu static lib even though bio2emu_port_init is never called from tests)
- [Phase 02]: FPGA node process_cmd functions tested directly (exported); msg layer tested via hook struct — clean boundary without iohook
- [Phase 02]: ezusb-iidx-16seg-emu and security required in test link deps for node_16seg and security_plug resolved symbols
- [Phase 02]: Bypass ac_io_emu_init: zero-initialize struct ac_io_emu + ac_io_in_init for ICCA dispatch-only tests (no iohook dependency)

### Pending Todos

None yet.

### Blockers/Concerns

- [Phase 2]: Hardware protocol traces for emulator tests may not exist in the project — assess before Phase 2 begins; if absent, use synthetic inputs and document the assumption
- [Phase 2]: doc/ directory may already have partial protocol specs — check before writing new reference material
- [Phase 3]: GhidraMCP Ghidra 11.x compatibility must be validated before variant selection (LaurieWired vs GhydraMCP vs GhidrAssistMCP)
- [Phase 1]: Wine version on CI runners (ubuntu-22.04 ships Wine 7.x) — determine if sufficient during Phase 1

## Session Continuity

Last session: 2026-03-01
Stopped at: Completed 02-05-PLAN.md — ICCA node tests and bug research artifacts for #345, #344, #351
Resume file: .planning/phases/02-protocol-reference-and-emulator-tests/02-05-SUMMARY.md
