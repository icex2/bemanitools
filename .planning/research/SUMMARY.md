# Project Research Summary

**Project:** Bemanitools AIDE (AI-Assisted Development Environment)
**Domain:** AI agent infrastructure for a C/Win32 DLL injection and reverse engineering codebase
**Researched:** 2026-03-01
**Confidence:** HIGH (stack and architecture derived from direct codebase analysis; MEDIUM for agentic RE workflow patterns)

## Executive Summary

Bemanitools is a mature C99/Win32 DLL injection toolkit for Konami rhythm game hardware emulation, cross-compiled with MinGW on Linux and executed under Wine. The AIDE project adds AI agent infrastructure on top of this existing stack — it is not a rewrite. The fundamental constraint shaping all decisions is the isolated/injected boundary: roughly half the codebase (protocol parsers, emulators, config, security) can be compiled and tested as standalone Windows PE executables under Wine with no game binary; the other half (hook DLLs, D3D9 hooks, AVS initialization) only executes inside a live game process and cannot be automatically tested. Every infrastructure decision must account for this split, because an agent that doesn't understand it will waste effort trying to test untestable code.

The recommended approach is additive and incremental: wrap the existing build system with a single agentic verification script, document what already exists (test patterns, module map, build workflow), then progressively add reference material that enables hardware emulator bug investigation. The most valuable capability — and the hardest — is protocol reference documentation for the ezusb-iidx, BIO2, and ACIO hardware protocols, which lets an agent verify emulator correctness against ground truth rather than guessing from source comments. The agentic RE tooling ecosystem (GhidraMCP for interactive Ghidra access, headless scripts for batch export) is active and mature enough to integrate, but it is a Phase 3 concern — meaningful only after the foundational verification loop exists.

The critical risks are all agent-context failures: wrong Win32 calling conventions in generated code (compiles, crashes at runtime), BT5/BT6 scope conflation (produces structurally incompatible changes), and treating decompiled output as ground truth (builds incorrect emulation behavior). All three are prevented by explicit agent context engineering — mandatory constraints in CLAUDE.md that are loaded on every session — not by technical infrastructure. CI test execution is the other urgent gap: the current workflow builds but never runs Wine tests, meaning regressions in tested modules go undetected until a human intervenes.

## Key Findings

### Recommended Stack

The existing stack (MinGW cross-compilation, GNUmake, Docker, Wine) requires no changes. AIDE layers on top: a shell wrapper (`scripts/aide-verify.sh`) that unifies build + Wine test execution into a single agent-callable command, `cppcheck` for static analysis without a compilation database, and Ghidra headless with GhidraMCP for reverse engineering workflows. No CMake, no C++ test frameworks, no ASAN — all are incompatible with the MinGW cross-compilation setup. The one optional addition is cmocka if tests need mock objects, though the existing `*emu` module pattern already handles hardware boundaries at a higher level.

**Core technologies:**
- Existing test framework (in-tree): unit tests for isolated C logic — already integrated in GNUmake, no new dependencies
- `cppcheck` 2.19+: static analysis on Linux host without compilation database — tolerates MinGW system header mismatches that break clang-tidy
- Ghidra 11.x (headless): batch decompilation of game DLLs for reference material — free, OSS, scriptable via Jython
- GhidraMCP (LaurieWired or GhydraMCP variant): exposes Ghidra analysis to Claude Code via MCP for interactive RE workflows
- Wine (system): runs cross-compiled test binaries on Linux — already used by `run-tests-wine.sh`, needs CI integration

### Expected Features

The "users" are AI coding agents, not Bemanitools end users. Features are evaluated by their impact on agent productivity on this codebase.

**Must have (table stakes — without these agents can't work effectively):**
- Single agentic verification command (`scripts/aide-verify.sh`) — removes two-command overhead, gives agents a single exit-code signal
- Build workflow documented (`.planning/reference/build-workflow.md`) — agent knows how to build/test/interpret without re-reading GNUmakefile each session
- Codebase module map (`.planning/codebase-map.md`) — isolated/injected flag per module, test coverage flag; agent orients in one read
- CLAUDE.md with mandatory constraints — isolated/injected boundary, Win32 calling convention rules, BT5 scope, test patterns; loaded on every session

**Should have (enable hardware emulator bug investigation):**
- Protocol reference docs for ezusb-iidx and BIO2 — ground truth for verifying emulator correctness
- Per-bug research artifacts (#345, #344, #351 and future) — eliminate context re-derivation overhead per session
- Emulator state machine unit tests (bio2emu, acioemu, ezusb-iidx-emu) — automated correctness signal for protocol emulator changes
- Wine CI integration — automated regression detection without human intervention

**Defer (v2+):**
- Ghidra MCP integration — high value, requires Ghidra project setup per game version; only meaningful after v1 infrastructure exists
- Game boot sequence expected logs — per-version expected log patterns for injected code regression checking
- Headless Ghidra batch reference export — useful but lower priority than interactive MCP once that's established
- Static analysis in CI (cppcheck) — useful signal, not blocking given the existing test suite

**Explicitly out of scope:**
- CMake integration, Google Test/Catch2, ASAN/UBSAN (all incompatible with MinGW cross-compilation setup)
- Full game-level test automation (requires proprietary binaries that cannot be in CI)
- Full decompilation committed as source (DMCA exposure, decompiler output is unreliable as ground truth)

### Architecture Approach

AIDE wraps Bemanitools without changing it. The architecture has four components: a Reference DB (`.planning/reference/`) storing protocol specs and API contracts for agent context; the existing Build Pipeline (GNUmake + Docker + Wine) unchanged; the existing Test Harness (`src/test/` with custom C macros) extended with emulator state machine tests; and a new Verification Layer (`scripts/aide-verify.sh`) that unifies build + test + structured output into a single agent entry point. The data flow is: bug artifact → protocol reference → implementation → unit tests → verification script → log output analysis → fix or iterate.

**Major components:**
1. Reference DB (`.planning/reference/`) — feeds agent context with protocol specs, API contracts, bug case files, game boot sequences
2. Verification Layer (`scripts/aide-verify.sh`) — build + Wine test execution with structured `[BUILD]`/`[TEST]`/`[ERROR]` output for agentic parsing
3. Test Harness expansion (`src/test/{bio2emu,acioemu,ezusb-iidx-emu}/`) — state machine unit tests for the emulator modules currently untested despite being where protocol bugs live
4. CLAUDE.md context engineering — mandatory agent constraints loaded on every session; the primary mechanism preventing the most serious failure modes

### Critical Pitfalls

1. **CI builds but never runs tests** — add Wine to CI image and run `make run-tests`; this is blocking for everything else. Current CI only runs `make build-docker`, meaning regressions in tested modules are invisible until manual discovery.

2. **Agent generates Win32 code with wrong calling conventions** — include explicit `__stdcall`/`WINAPI` rules in CLAUDE.md before any hook code generation; wrong conventions compile silently but corrupt the stack at runtime inside the game process.

3. **Agent conflates BT5 architecture with BT6 refactoring plans** — scope every session explicitly to BT5; list BT6 issue numbers (#290-297, #305) as off-limits; recovery from this is HIGH cost (revert and re-implement).

4. **Decompiled symbol reference treated as ground truth** — mark all Ghidra/IDA output as "UNVERIFIED DECOMPILER OUTPUT — advisory only"; include this caveat in agent context; protocol emulation code must be validated against hardware traces, not decompiler pseudo-C.

5. **Testing the emulator stub rather than the protocol** — every emulator test needs at least one reference fixture from a real hardware packet capture; tests that only verify current emulator behavior detect nothing when the emulator itself is wrong.

## Implications for Roadmap

Based on combined research, a four-phase structure emerges from the dependency graph in FEATURES.md and the pitfall-to-phase mapping in PITFALLS.md.

### Phase 1: Agent Foundation

**Rationale:** All other phases depend on agents being able to compile, test, and verify changes reliably. Without this foundation, agents produce output they cannot validate. The entire value proposition of AIDE collapses if the verification loop doesn't work. This phase is all LOW complexity, all CLAUDE.md + documentation + thin shell scripting.

**Delivers:** A working agent session: orientation → code change → build → test → interpret results, without constant human intervention on tooling.

**Addresses:** All five P1 table stakes features (verification command, build workflow docs, codebase module map, CLAUDE.md constraints, test pattern documentation).

**Avoids:** Pitfalls 2 (CI never runs tests), 4 (wrong calling conventions), 5 (BT5/BT6 conflation) — all prevented by CLAUDE.md and the verification script existing before any code generation happens.

**Key deliverables:**
- `scripts/aide-verify.sh` — single agent verification command
- `.planning/reference/build-workflow.md` — build/test interpretation guide
- `.planning/codebase-map.md` — module-level orientation (isolated/injected flags, test coverage)
- CLAUDE.md updated with mandatory constraints (calling conventions, BT5 scope, test patterns)
- CI extended to run Wine tests (Wine added to CI job or `Dockerfile.test`)

### Phase 2: Protocol Reference and Emulator Tests

**Rationale:** The dominant bug class in Bemanitools is hardware protocol emulation bugs. Fixing them requires knowing what the wire protocol specifies (not just what the code currently does). Protocol docs must precede emulator tests — you cannot write meaningful state machine tests without knowing the correct expected output. Per-bug artifacts eliminate context re-derivation overhead that currently makes every agent session on a recurring bug start from scratch.

**Delivers:** The ability for an agent to investigate and fix hardware emulator bugs with automated verification signal. This is where AIDE moves from "agent can compile" to "agent can reason about protocol correctness."

**Addresses:** Protocol reference docs (ezusb-iidx, BIO2, ACIO), per-bug research artifacts, emulator state machine unit tests.

**Avoids:** Pitfall 1 (testing the stub rather than the protocol) — protocol reference docs enable tests to be written against wire-correct expected output, not just current emulator behavior.

**Key deliverables:**
- `.planning/reference/protocols/ezusb-iidx.md` — IIDX ezusb message format reference
- `.planning/reference/protocols/bio2.md` — BIO2 serial protocol reference
- `.planning/reference/protocols/acio.md` — ACIO serial protocol reference
- `.planning/reference/bugs/{issue}.md` for active bugs (#345, #344, #351)
- `src/test/bio2emu/`, `src/test/acioemu/`, `src/test/ezusb-iidx-emu/` — state machine tests with hardware trace fixtures

### Phase 3: Advanced Agent Context

**Rationale:** After protocol reference material and emulator tests exist, the agent can fix isolated-tier bugs reliably. Phase 3 addresses the injected tier — hook code and game boot sequences — where automated testing is impossible but log-based verification is feasible. This phase also adds static analysis as a second CI signal beyond test pass/fail.

**Delivers:** Coverage of the injected code tier via log pattern verification and API contract documentation, plus cppcheck as a static analysis gate.

**Addresses:** Game boot sequence expected logs, Bemanitools API contract docs, cppcheck CI integration.

**Avoids:** Pitfall 6 (decompiled code as ground truth) — API contract docs are written from the header files and observed behavior, not decompiler output.

**Key deliverables:**
- `.planning/reference/games/{popn,iidx}/boot-expected.txt` — log patterns for successful boot verification
- `.planning/reference/apis/iidxio.md`, `ddrio.md`, `eamio.md` — API contracts for I/O implementations
- cppcheck added as Makefile target and CI step

### Phase 4: Ghidra RE Integration

**Rationale:** GhidraMCP enables agents to query game binary internals directly — decompiled functions, cross-references, import tables — without manual export. This is the highest-value capability for reverse engineering new hardware protocols or investigating game binary behavior, but it requires all earlier infrastructure to exist and a one-time Ghidra project setup per game version. It cannot be in CI (proprietary binaries stay local). This is developer workstation capability only.

**Delivers:** Interactive game binary analysis via Claude Code MCP, and optional headless batch export for static reference material.

**Addresses:** Ghidra MCP integration (LaurieWired or GhydraMCP), headless Ghidra batch reference export.

**Avoids:** Pitfall 6 (decompiled code treated as ground truth) — the convention of marking all Ghidra output as advisory must be established before this integration goes live.

**Key deliverables:**
- GhidraMCP installed and configured against a Ghidra project containing target game DLLs
- MCP client configuration in Claude Code settings
- Headless batch export scripts for static reference files
- All exported material marked "UNVERIFIED DECOMPILER OUTPUT — advisory only"

### Phase Ordering Rationale

- Phase 1 must come first: without a working verification loop, agents cannot safely iterate on anything
- Phase 2 requires Phase 1: emulator tests must be runnable via `aide-verify.sh`; protocol docs must exist before tests to ensure tests verify correctness, not just current behavior
- Phase 3 is independent of Phase 2 but lower priority: log verification for injected code is useful but not blocking while isolated-tier bugs dominate the backlog
- Phase 4 requires Phase 1 minimally and Phase 2 ideally: GhidraMCP is most useful when the agent already has protocol context to interpret decompiler output; without protocol docs, the agent sees function signatures without understanding what they mean

### Research Flags

Phases needing deeper research during planning:
- **Phase 2:** Protocol reference documentation for ezusb-iidx, BIO2, and ACIO requires reading source + supplementing from any available hardware documentation. The `doc/` directory may have partial specs; assess coverage before writing new reference material to avoid duplication.
- **Phase 4:** GhidraMCP variant selection (LaurieWired vs GhydraMCP vs GhidrAssistMCP) should be validated against current Ghidra 11.x compatibility before committing to an implementation.

Phases with established patterns (skip research-phase):
- **Phase 1:** All deliverables are documentation and shell scripting against a well-understood existing build system. No novel tooling decisions.
- **Phase 3:** cppcheck integration is documented in STACK.md with specific flags; log pattern verification is a defined format with no external dependencies.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | MEDIUM | Core decisions (no CMake, no C++ frameworks, cppcheck over clang-tidy) are HIGH confidence from codebase constraints. GhidraMCP variant selection is MEDIUM — active projects with version compatibility to verify. |
| Features | HIGH | Grounded in direct codebase analysis. P1 features derived from existing infrastructure. P2/P3 features derived from active bug backlog and isolated/injected boundary analysis. |
| Architecture | HIGH | Derived entirely from direct source analysis. Component boundaries, data flow, and build patterns all verified against actual files. |
| Pitfalls | HIGH (codebase) / MEDIUM (agentic RE) | CI and Wine pitfalls are HIGH confidence — directly observable in current workflow files. Agent calling convention and BT5/BT6 scope pitfalls are HIGH confidence — well-documented LLM failure modes. Hardware trace fixture pitfall is MEDIUM — pattern is sound but depends on whether real hardware captures exist or can be created. |

**Overall confidence:** HIGH for Phase 1 and 2; MEDIUM for Phase 4 (Ghidra integration).

### Gaps to Address

- **Hardware protocol traces:** Phase 2 emulator tests need at least one byte-level capture per emulated device as a fixture. It is unclear whether any such captures exist in the project. This must be assessed before Phase 2 begins; if no captures exist, the testing strategy must be adjusted to "test protocol state transitions with synthetic inputs and document the assumption."
- **doc/ protocol coverage:** The `doc/` directory may already have partial protocol specs. Assess existing documentation before writing new reference material to avoid duplication and ensure consistency.
- **Wine version on CI runners:** `ubuntu-22.04` ships Wine 7.x via `apt`. Whether this is sufficient for the test suite or whether WineHQ staging PPA is needed should be determined during Phase 1 implementation, not assumed.
- **GhidraMCP Ghidra 11.x compatibility:** All three MCP variants need version compatibility checked against Ghidra 11.x before Phase 4 selection.

## Sources

### Primary (HIGH confidence)
- Direct codebase analysis — `src/test/`, `GNUmakefile`, `run-tests-wine.sh`, `dist/test/run-tests.sh`, `.github/workflows/build-master.yaml`, `Dockerfile.build`
- `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/TESTING.md`, `.planning/codebase/CONCERNS.md`
- cppcheck official manual — `--platform` flag and MinGW support
- Ghidra official headless docs — `analyzeHeadless` API

### Secondary (MEDIUM confidence)
- https://github.com/LaurieWired/GhidraMCP — Ghidra MCP plugin, active project
- https://github.com/starsong-consulting/GhydraMCP — multi-instance Ghidra MCP with REST API
- https://github.com/jtang613/GhidrAssistMCP — 34-tool SSE/HTTP variant
- Martin Fowler — Context Engineering for Coding Agents
- Spotify Engineering — Feedback Loops for Background Coding Agents
- Cross-building GROMACS with MinGW and Wine (2025) — Wine + MinGW cross-compilation patterns
- LLM hallucination in code generation (ACM 2025)

### Tertiary (LOW confidence)
- https://github.com/galoget/ghidra-headless-scripts — community headless batch scripts; verify before use
- Win32 calling convention training data coverage in LLMs — inferred from known LLM failure modes, not measured directly

---
*Research completed: 2026-03-01*
*Ready for roadmap: yes*
