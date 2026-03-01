---
phase: 01-agent-foundation
plan: "02"
subsystem: docs
tags: [documentation, agent, orientation, codebase-reference, conventions]

requires: []

provides:
  - "doc/agent-reference.md — module tier map, test coverage table, and build quick-start for agent orientation"
  - "CLAUDE.md — project-level agent guardrails (scope, conventions, do-not-touch list)"

affects: [all-phases]

tech-stack:
  added: []
  patterns:
    - "Module tier classification: Tier 1 (pure logic), Tier 2 (emulation), Tier 3 (hardware-dependent)"
    - "Agent constraint file at repo root (CLAUDE.md) for project-level guardrails"

key-files:
  created:
    - doc/agent-reference.md
    - CLAUDE.md
  modified: []

key-decisions:
  - "agent-reference.md links out to existing docs rather than duplicating content — keeps it concise and maintainable"
  - "CLAUDE.md is self-contained with no dependency on personal ~/.claude/rules/ — works for any contributor"
  - "Do-not-touch list explicitly names GNUmakefile, root Module.mk, .clang-format, imports/ to prevent build system breakage"
  - "aide-verify.sh is the single verification entry point referenced from both files"

patterns-established:
  - "Tier classification: any new module should be assigned a tier before writing tests"
  - "Verification gate: all agents run scripts/aide-verify.sh before every commit"
  - "Orientation flow: CLAUDE.md → doc/agent-reference.md → doc/development.md / doc/architecture.md"

requirements-completed: [AIDE-04]

duration: 2min
completed: 2026-03-01
---

# Phase 1 Plan 2: Agent Documentation Summary

**Agent orientation docs: three-tier module map in doc/agent-reference.md and strict guardrails in CLAUDE.md pointing agents at aide-verify.sh as the single verification entry point**

## Performance

- **Duration:** ~2 min
- **Started:** 2026-03-01T11:06:44Z
- **Completed:** 2026-03-01T11:08:30Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Created `doc/agent-reference.md` with Tier 1/2/3 module classification, test coverage table, hook architecture overview, and build quick-start
- Created `CLAUDE.md` at repo root constraining agents to BT5 scope, C99 conventions, test-before-commit workflow, and explicit do-not-touch list

## Task Commits

1. **Task 1: Create doc/agent-reference.md** - `2a40dca` (docs)
2. **Task 2: Create project-level CLAUDE.md** - `8ef0f60` (docs)

**Plan metadata:** (see final commit below)

## Files Created/Modified

- `doc/agent-reference.md` — Agent-oriented codebase reference: module tiers, test coverage map, hook architecture, key conventions
- `CLAUDE.md` — Project-level agent constraints: scope (BT5 only), verification workflow, coding conventions, do-not-touch list, build quick-reference

## Decisions Made

- Linked out to existing `doc/architecture.md` and `doc/development.md` rather than duplicating content — keeps `agent-reference.md` under 200 lines and stays maintainable
- CLAUDE.md written to be self-contained (no dependency on personal `~/.claude/rules/`) so it works for any contributor or agent deployment
- Tier 2 includes modules with tests (d3d9hook, iidxhook8) alongside untested emulation modules — both are hardware-free and Wine-testable, so they belong together

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- Agent orientation infrastructure complete: any agent can read CLAUDE.md → doc/agent-reference.md and know which modules are testable, how to build, and what not to touch
- Phase 2 (emulator testing) can begin — agent-reference.md already classifies Tier 2 modules as the testing target

---
*Phase: 01-agent-foundation*
*Completed: 2026-03-01*
