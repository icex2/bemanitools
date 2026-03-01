# Roadmap: Bemanitools 5 — Bug Triage & Maintenance

## Overview

This maintenance cycle builds AI agent infrastructure first, then uses that infrastructure to fix regressions, land pending contributions, and triage the issue backlog toward a 5.50 release. The AIDE phases establish the verification loop and reference material that make agent-assisted bug investigation reliable. Bug fixing phases come after because they depend on that foundation. The final phase closes the issue tracker to a clean state.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Agent Foundation** - Establish the agentic verification loop (build, test, interpret, repeat)
- [ ] **Phase 2: Protocol Reference and Emulator Tests** - Give agents ground truth for hardware protocol correctness
- [ ] **Phase 3: Ghidra RE Integration** - Enable interactive game binary analysis via MCP
- [ ] **Phase 4: Regressions** - Fix the highest-priority game boot regressions
- [ ] **Phase 5: Bug Fixes and Contributions** - Fix remaining bugs and merge community PRs
- [ ] **Phase 6: Triage and Release** - Close invalid issues, label backlog, ship 5.50

## Phase Details

### Phase 1: Agent Foundation
**Goal**: Agents can build, test, and verify changes without constant human tooling intervention
**Depends on**: Nothing (first phase)
**Requirements**: AIDE-01, AIDE-02, AIDE-04
**Success Criteria** (what must be TRUE):
  1. Running `scripts/aide-verify.sh` produces structured `[BUILD]`/`[TEST]`/`[PASS]`/`[FAIL]` output an agent can parse without reading GNUmakefile
  2. CI runs Wine tests on every push and fails the build on test regressions (not just build failures)
  3. An agent starting a fresh session can orient to the codebase — which modules are isolated vs injected, which have test coverage — by reading a single reference file
  4. CLAUDE.md constrains agents to BT5 scope, Win32 calling conventions, and correct test patterns before any code generation
  5. Existing unit tests can be run under Wine and all pass
**Plans**: TBD

### Phase 2: Protocol Reference and Emulator Tests
**Goal**: Agents have ground-truth protocol reference material and automated correctness signals for the hardware emulator tier
**Depends on**: Phase 1
**Requirements**: AIDE-03
**Success Criteria** (what must be TRUE):
  1. Protocol reference documents exist for ezusb-iidx, BIO2, and ACIO with message formats accurate enough to write tests against
  2. State machine unit tests exist for bio2emu, acioemu, and ezusb-iidx-emu and pass via `aide-verify.sh`
  3. Per-bug research artifacts exist for the three active regressions (#345, #344, #351) so an agent starting work on any of them does not re-derive context from scratch
  4. Emulator tests verify against declared expected behavior, not just current emulator output
**Plans**: TBD

### Phase 3: Ghidra RE Integration
**Goal**: Agents can query decompiled game binary internals interactively via MCP without manual export steps
**Depends on**: Phase 2
**Requirements**: AIDE-05
**Success Criteria** (what must be TRUE):
  1. GhidraMCP is installed and a Claude Code MCP client config points to a live Ghidra project containing at least one target game DLL
  2. An agent can query decompiled function bodies and cross-references via MCP tool calls without Ghidra GUI interaction
  3. All Ghidra-exported material is marked "UNVERIFIED DECOMPILER OUTPUT — advisory only" in any reference files produced
  4. Headless batch export scripts produce static reference files for offline use
**Plans**: TBD

### Phase 4: Regressions
**Goal**: The three highest-priority game boot regressions are diagnosed and fixed using the AIDE infrastructure
**Depends on**: Phase 2
**Requirements**: REGR-01, REGR-02, REGR-03
**Success Criteria** (what must be TRUE):
  1. Pop'n Music 15-18 boots and runs correctly on the current build (v5.43→5.44 regression resolved, #345/#341/#338 closed)
  2. IIDX 11-15 song selection timing behaves correctly on modern hardware without manual workarounds (#344 closed)
  3. IIDX tricoro boots when configured with a CN network address (#351 closed)
**Plans**: TBD

### Phase 5: Bug Fixes and Contributions
**Goal**: Remaining open bugs are fixed and community PRs are reviewed, approved, and merged
**Depends on**: Phase 1
**Requirements**: BUGF-01, BUGF-02, BUGF-03, CONT-01, CONT-02, CONT-03
**Success Criteria** (what must be TRUE):
  1. IIDX 12 Happy Sky launches without a silent failure — the cause is identified, fixed, and the fix is verified (#356 closed)
  2. IIDX 30 default logger configuration produces valid output without errors (#306 closed)
  3. DDR P2 keyboard card insertion works as expected (#347 closed)
  4. Smartcard support, MDXF, and minimaid+HID PRs are each reviewed, any requested changes addressed, and merged (#355, #350, #358 closed)
**Plans**: TBD

### Phase 6: Triage and Release
**Goal**: The issue tracker reflects accurate priority and state, and the 5.50 release ships
**Depends on**: Phase 5
**Requirements**: TRIA-01, TRIA-02
**Success Criteria** (what must be TRUE):
  1. Every open GitHub issue has at least one label indicating type (bug, feature, support, wontfix) and priority (P0–P3)
  2. All support and invalid issues are closed with a guidance comment explaining why
  3. The 5.50 release is tagged with a changelog covering all fixes and merged contributions from this cycle
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Agent Foundation | 0/TBD | Not started | - |
| 2. Protocol Reference and Emulator Tests | 0/TBD | Not started | - |
| 3. Ghidra RE Integration | 0/TBD | Not started | - |
| 4. Regressions | 0/TBD | Not started | - |
| 5. Bug Fixes and Contributions | 0/TBD | Not started | - |
| 6. Triage and Release | 0/TBD | Not started | - |
