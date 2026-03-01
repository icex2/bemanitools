# Feature Research

**Domain:** AI-agent-friendly development infrastructure for a C/Win32 DLL injection / reverse engineering codebase
**Researched:** 2026-03-01
**Confidence:** HIGH (grounded in direct codebase analysis + verified against current tooling landscape)

---

## Context

The "users" here are AI coding agents (primarily Claude Code), not end users of Bemanitools. The question is: what features must the development environment have for an agent to be effective on this codebase? Every feature decision is shaped by three hard constraints from the codebase:

1. **The isolated/injected split.** Code in `src/main/util/`, `security/`, `cconfig/`, and hardware emulators (`*emu/`) can be compiled and tested as standalone Windows PE executables under Wine on Linux without any game binary. Code in `*hook*/dllmain.c` and D3D9 hooks runs exclusively inside a live game process. This boundary dictates what can be tested automatically and what cannot.

2. **MinGW cross-compilation, not CMake.** The build system is GNU Make + `i686-w64-mingw32-gcc`. Any feature that requires CMake, a compilation database, or native compilation is ruled out.

3. **No proprietary binaries in the repo.** The Konami game DLLs an agent needs to understand are legally required to stay off-repo. Reference material must be derived and curated, not the binaries themselves.

---

## Feature Landscape

### Table Stakes (Must Have or Agents Can't Work Effectively)

These are the features an agent needs before it can do anything useful. Without them, the agent either can't compile code, can't verify its changes, or lacks the domain context to understand what it's modifying.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Fast local build feedback** | An agent needs `make build/bin/indep-32/{module}.exe` to work in seconds on a host with MinGW installed. Without fast incremental builds, the fix-verify loop becomes unusable. The current Docker-only path takes 5-10 minutes. | LOW | MinGW packages already exist on Debian/Ubuntu. Create a `Dockerfile.dev` for interactive dev or document native MinGW install. The GNUmakefile already supports single-module targets. Nothing to build, just document. |
| **Single agentic verification command** | Agents need one command that answers "did my change break anything?" — build + run tests + report exit code. The current two-step (`make` then `run-tests-wine.sh`) is fine for humans but requires agents to manage state. | LOW | A `scripts/aide-verify.sh` wrapper: build module → run Wine tests → emit structured output with `[BUILD]`/`[TEST]`/`[ERROR]` prefixes → exit 0/non-0. The hard work (MinGW, Wine, test runner) already exists. This is a thin shell wrapper. |
| **Wine test execution documented for agents** | The existing `run-tests-wine.sh` works but the agent needs to know: what Wine version, what `WINEPREFIX`/`WINEARCH` settings, what a passing run looks like vs. a failing one, and how to run a single test binary directly (`wine build/bin/indep-32/security-id-test.exe`). | LOW | Write `.planning/reference/build-workflow.md`. One file, ~2 pages. Covers: build single module, run full suite, run one test, interpret output, expected `wine --version`. |
| **Codebase map for agent orientation** | A 150K LOC codebase with 80+ modules is too large for an agent to orient in from file listings alone. The agent needs a machine-readable map: what each `src/main/{module}/` does, which modules are isolated vs. injected, what the existing tests cover. Without this, every session wastes 20-30% of context window re-reading ARCHITECTURE.md. | MEDIUM | Extend CLAUDE.md or create a compact `.planning/codebase-map.md` — one line per module, isolated/injected flag, test coverage flag. Derive from existing `.planning/codebase/ARCHITECTURE.md`. Cross-reference: AIDE-04 requirement. |
| **Isolated/injected boundary explicitly documented** | This is the single most critical architectural concept for an agent. An agent that doesn't know this boundary will try to write unit tests for `dllmain.c` (impossible) or assume D3D9 hook code can be tested without a game (wrong). Must be in CLAUDE.md or the agent's immediate context. | LOW | Already documented in `.planning/research/ARCHITECTURE.md`. Needs to be surfaced in CLAUDE.md as a mandatory constraint, not buried in a research file the agent may not load. |
| **Existing test patterns documented** | The custom `TEST_MODULE_BEGIN/END` + `check_*` macro framework is non-standard. An agent trained on gtest or catch2 will not know how to add tests. The pattern must be explicitly shown: Module.mk declaration, test file structure, what assertion macros exist, how to wire a new test into the runner. | LOW | Already captured in `.planning/codebase/TESTING.md`. Needs CLAUDE.md reference so agents load it before writing any test code. |
| **Win32 calling convention rules in agent context** | This is a safety-critical table stake for hook code. An agent generating `__cdecl` functions where `WINAPI` (`__stdcall`) is required produces code that compiles but crashes at runtime — and only when running inside the game. This failure mode is invisible without hardware testing. | LOW | A short "Win32 hook conventions" section in CLAUDE.md or a dedicated `.planning/reference/conventions.md`. Must include: `WINAPI` = `__stdcall`, when it's required (all hook functions, COM methods, Win32 API callbacks), examples from the existing codebase. |
| **BT5 scope explicitly bounded** | The codebase contains references to the BT6 refactoring effort (issues #290-297, #305). An agent without explicit scoping will cross-contaminate BT5 fixes with BT6 architectural patterns, producing code incompatible with the current build. | LOW | One section in CLAUDE.md: "This codebase is BT5. Do not introduce BT6 patterns, abstractions, or module boundaries. Issues #290-297 and #305 are out of scope." |

### Differentiators (Enable Advanced Agentic RE Workflows)

These features go beyond "agent can compile and test" to "agent can reason about hardware protocol correctness" — which is the core capability needed for fixing hardware emulation bugs.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Protocol reference documentation** | Hardware protocol bugs (the dominant bug class in Bemanitools) require knowing what the wire protocol actually specifies, not just what the code currently does. Reference docs for ezusb-iidx, BIO2, ACIO wire protocols let an agent verify a message handler against the ground truth rather than guessing from source comments. Without this, the agent is playing telephone — inferring protocol intent from incomplete implementation. | HIGH | Start with ezusb-iidx (IIDX is the biggest game family with the most bugs). Write `.planning/reference/protocols/ezusb-iidx.md` by reading `src/main/ezusb-iidx/` and `src/main/ezusb-iidx-emu/`. Supplement with BIO2, ACIO. Each doc: message format tables, state machine diagrams, known edge cases. High effort, high payoff. |
| **Game boot sequence expected logs per version** | For injected code that cannot be unit tested, the only automated verification is log comparison: run the game, capture the log, diff against known-good. Expected boot log patterns per game version enable an agent to detect regressions in hook initialization code without requiring a human to watch the game boot. | MEDIUM | Write `.planning/reference/games/{game}/boot-expected.txt` — line patterns that must appear in sequence in a successful boot log. Start with popn (active regression) and IIDX (multiple bugs). The `log.h` infrastructure already writes to file; this is reference data only. |
| **Per-bug research artifacts** | Each active bug has context: symptom, affected code paths, relevant protocol sections, what was tried. Without this, every agent session on a bug re-derives the same context from scratch. A bug artifact at `.planning/reference/bugs/{issue}.md` is the agent's "case file" — it loads this before reading source. | MEDIUM | One file per active bug. Start with #345/#341/#338 (popn regression), #344 (IIDX timing). Format: symptom → suspected module → relevant protocol section → what changed in v5.44. Requires human to seed initial context; agent can update as investigation proceeds. |
| **Emulator state machine unit tests** | The hardware emulators (`ezusb-iidx-emu/`, `bio2emu/`, `acioemu/`) are isolated modules that contain the actual protocol logic. They currently have zero unit tests despite being in the isolated tier. Agents making protocol fixes have no automated way to verify correctness. Adding tests for message node state transitions gives agents a green/red signal for protocol emulator changes. | HIGH | Add test files under `src/test/` for `bio2emu/`, `acioemu/`, and `ezusb-iidx-emu/` message nodes. Follow existing `TEST_MODULE_BEGIN/END` pattern. Each test exercises a message node with known input bytes and checks the output state. Requires reading the protocol to write meaningful tests — this is where protocol docs pay off. |
| **Static analysis in CI (cppcheck)** | Agents cannot run the game binary; they rely on compiler feedback and tests. Static analysis adds a third signal: code that compiles but has undefined behavior, unreachable code, or suspicious patterns. `cppcheck` works against C99 source on a Linux host without a compilation database, tolerates missing MinGW system headers, and integrates directly into `make`. | MEDIUM | Add `cppcheck src/main src/test --enable=all --suppress=missingIncludeSystem --platform=win32A -j4` as a Makefile target and GitHub Actions step. The cppcheck `--platform=win32A` flag improves Windows-specific checks. MEDIUM complexity because cppcheck 2.19+ may need PPA install on ubuntu-22.04. |
| **Ghidra MCP integration for interactive RE** | Agents fixing hooks against game DLLs need to understand what the game binary actually calls. GhidraMCP (LaurieWired or GhydraMCP variant) exposes Ghidra's analysis to Claude Code via MCP: the agent can query decompiled function signatures, trace cross-references, and look up imports without manual export steps. This closes the gap between "reading hook source" and "understanding what the game expects." | HIGH | Requires Ghidra project with the target game DLLs loaded (done once per game version). Install GhidraMCP plugin, configure Claude Code's MCP settings. Per-session: the agent can call `decompile_function`, `list_imports`, `get_cross_references` directly. Proprietary binaries stay local, only analysis results enter context. |
| **Headless Ghidra batch reference export** | For cases where interactive MCP isn't needed, a one-time headless analysis run can produce reference headers: function signatures, export tables, and key struct layouts from game DLLs. These exports go into `.planning/reference/` as static files, usable offline. | MEDIUM | `analyzeHeadless` with a Python post-script that exports function names, parameter types, and return types as JSON/Markdown. Commit extracted reference (not the DLL). Reference file clearly marked "UNVERIFIED DECOMPILER OUTPUT — advisory only." |
| **Wine CI integration** | Tests run locally under Wine but CI (`build-master.yaml`) only runs `make build-docker` — no test execution. CI that runs tests catches regressions without human intervention, enables agentic PR validation, and confirms Wine test compatibility on a known-good environment. | MEDIUM | Extend `.github/workflows/build-master.yaml`: install `wine` and `xvfb`, set `WINEARCH=win32` and `WINEPREFIX=/tmp/wine`, run `make run-tests`. Or create `Dockerfile.test` extending the build image with Wine (more reproducible). MEDIUM complexity because `xvfb` and Wine headless setup has gotchas (documented in PITFALLS.md). |
| **Structured log output for agentic parsing** | Log output from Wine test runs uses the existing `log.h` format. For agentic use, a structured prefix convention (`[BUILD] success`, `[TEST] PASS module-name`, `[ERROR] ...`) makes it trivially parseable without regex. The agent knows immediately whether to iterate or commit. | LOW | Thin wrapper in `scripts/aide-verify.sh`. No changes to the existing build system or test framework. |

### Anti-Features (Deliberately NOT Build)

Things that look useful but create more problems than they solve in this context.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| **Full decompilation of game binaries as committed source** | Agents need to understand game binary behavior; having the decompiled C in the repo seems convenient. | Legal: decompiled output is a derivative of proprietary Konami binaries, creating DMCA exposure. Technical: decompiler output is pseudo-C with wrong types, missing calling conventions, and no variable semantics — treating it as authoritative causes more bugs than it prevents. | Commit curated reference files: exported function signatures, protocol behavior docs, and API contracts derived from observation. Mark everything from decompilers as advisory. Keep raw Ghidra output local, never committed. |
| **CMake integration for test infrastructure** | CMake enables clang-tidy, better IDE integration, and some mock frameworks (cmocka). | Introduces a second parallel build system. All existing GNUmakefile conventions become ambiguous. MinGW toolchain configuration in CMake requires a toolchain file that adds significant complexity with no payoff. Breaks the existing `make` → `run-tests-wine.sh` pipeline that agents already use. | Extend the existing GNUmakefile + Module.mk pattern. It already does everything needed. |
| **Google Test / Catch2** | Industry-standard test frameworks; AI agents are trained on them. | Both are C++ frameworks; Bemanitools tests are C99. Requiring a C++ compiler in the cross-compilation toolchain adds dependencies. The existing custom macro framework (`TEST_MODULE_BEGIN/END`, `check_*`) is functionally equivalent for this codebase's needs. | Use the existing framework. Document it in CLAUDE.md so agents learn it rather than importing gtest patterns. |
| **ASAN/UBSAN in cross-compiled binaries** | Catches memory safety bugs at runtime in Wine. | AddressSanitizer and UBSanitizer require runtime support libraries compiled into the binary. Their MinGW implementations are incomplete and Wine's emulation of the sanitizer runtime is unreliable — results are false positives and false negatives, not useful signal. | Use `cppcheck` for static analysis. Wine-run tests catch obvious crashes (abort, access violation, wrong exit code). |
| **Full game-level test automation (running bm2dx.exe in CI)** | True end-to-end regression testing would catch the most important bugs. | Requires proprietary game binaries that cannot be in the repo or CI. Game boot requires specific hardware emulation, network configuration, and timing. The Konami AVS framework has anti-debugger and anti-VM measures that make automated execution unreliable. False positives would swamp the signal. | Log-based verification: capture expected boot log patterns from a known-good manual run, compare against actual logs from developer testing. Use unit tests for emulator logic. |
| **Radare2/r2 as an alternative to Ghidra** | Scriptable via Python, some prefer the UX. | Community documentation for batch analysis is weaker. The `r2lang` scripting API surface is smaller and less consistent than Ghidra's Jython/Java scripting. GhidraMCP implementations for r2 don't exist at the quality level of the Ghidra variants. If switching from Ghidra, choose Binary Ninja (better API) not r2. | Use Ghidra headless. It's free, well-documented, has mature Python scripting via Jython, and the GhidraMCP ecosystem is active (multiple implementations). |
| **Per-session agent memory systems (vector stores, embeddings)** | An agent that "remembers" previous sessions sounds useful. | The `.thebrain/` / `.planning/reference/` file-based approach already solves this. Vector stores add infrastructure complexity, require embedding model setup, and add latency. File reads are instant and deterministic. For a single-developer project, a well-organized markdown reference directory outperforms a vector store. | Use markdown reference files in `.planning/reference/`. Keep them scoped (one file per protocol, bug, or API). This is the approach the existing `.thebrain/` system already uses. |

---

## Feature Dependencies

```
[Single agentic verification command]
    └──requires──> [Fast local build feedback]
                       └──requires──> [MinGW installed locally or Dockerfile.dev]

[Emulator state machine unit tests]
    └──requires──> [Protocol reference documentation]
                       (can't write meaningful tests without knowing what correct output is)

[Wine CI integration]
    └──requires──> [Single agentic verification command]
                       (CI should run the same command agents run locally)

[Ghidra MCP integration]
    └──requires──> [Ghidra project with target DLLs loaded]
                   (one-time setup per game version; DLLs stay local)

[Per-bug research artifacts]
    ──enhances──> [Emulator state machine unit tests]
                  (bug artifacts identify which state machines need tests)

[Protocol reference documentation]
    ──enhances──> [Ghidra MCP integration]
                  (Ghidra analysis is more useful when agent has protocol context to interpret it)

[Game boot sequence expected logs]
    ──requires──> [Codebase map for agent orientation]
                  (agent needs to know which log lines come from which module)

[Static analysis in CI (cppcheck)]
    ──enhances──> [Wine CI integration]
                  (provides a second signal beyond test pass/fail)

[Isolated/injected boundary documented] ──conflicts──> [Full game-level test automation]
    (once agents understand the boundary, they stop expecting game-level tests to exist)
```

### Dependency Notes

- **Emulator state machine unit tests requires protocol reference documentation:** You cannot write a meaningful protocol state machine test without knowing what the correct wire output is. The test data (expected bytes, expected state transitions) comes from the protocol reference. Building tests before docs produces tests that only verify current (possibly wrong) behavior.

- **Ghidra MCP integration requires a Ghidra project with target DLLs loaded:** This is a one-time manual setup step per game version. The DLLs (game binaries) are kept locally and never committed. Once the project is loaded and analyzed, GhidraMCP serves queries. This is not a CI dependency — it's a developer workstation capability.

- **Per-bug research artifacts enhance emulator state machine unit tests:** Bug artifacts identify which modules are implicated, which helps prioritize which state machines get tests first. This avoids writing tests for modules that aren't in the bug chain.

---

## MVP Definition

### Launch With (v1 — Enable Basic Agentic Work)

The minimum set that makes an agent meaningfully productive on this codebase without constant manual intervention.

- [ ] **Single agentic verification command** (`scripts/aide-verify.sh`) — removes the two-command mental overhead and gives agents a single exit-code signal
- [ ] **Build workflow documented** (`.planning/reference/build-workflow.md`) — agent knows how to build, test, interpret output without re-reading GNUmakefile every session
- [ ] **Codebase module map** (compact `.planning/codebase-map.md`) — isolated/injected flag per module, test coverage flag; agent can orient in one read
- [ ] **CLAUDE.md with mandatory constraints** — isolated/injected boundary, Win32 calling convention rules, BT5 scope, how to add tests; loaded on every session
- [ ] **Existing test patterns documented** (CLAUDE.md reference to `.planning/codebase/TESTING.md`) — agent can add tests without guessing the macro names

These five are all LOW complexity and purely documentation/scripting. They enable the agent to compile, test, and verify basic C logic changes with confidence.

### Add After Validation (v1.x — Enable RE Workflow)

- [ ] **Protocol reference documentation for ezusb-iidx and BIO2** — enables meaningful hardware emulator bug investigation; HIGH complexity, highest payoff
- [ ] **Per-bug research artifacts for active bugs** (#345, #344, #351) — eliminates context re-derivation overhead; MEDIUM complexity
- [ ] **Emulator state machine unit tests** (bio2emu, acioemu, ezusb-iidx-emu) — provides automated protocol correctness signal; HIGH complexity; requires protocol docs first
- [ ] **Wine CI integration** — automated regression detection; MEDIUM complexity; eliminates manual verification step before merging

### Future Consideration (v2+ — Advanced Agentic RE)

- [ ] **Ghidra MCP integration** — interactive game binary analysis; HIGH complexity; requires Ghidra project setup per game version; extremely high value once active, but needs v1 infrastructure first
- [ ] **Headless Ghidra batch reference export** — one-time analysis per game DLL set; MEDIUM complexity; lower priority than interactive MCP
- [ ] **Game boot sequence expected logs** — per-version expected log patterns; MEDIUM complexity; only useful after v1.x makes the rest of the workflow stable
- [ ] **Static analysis CI (cppcheck)** — low priority; the existing test suite catches most correctness issues; useful but not blocking

---

## Feature Prioritization Matrix

| Feature | Agent Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Single agentic verification command | HIGH | LOW | P1 |
| Build workflow documented | HIGH | LOW | P1 |
| Codebase module map | HIGH | LOW | P1 |
| CLAUDE.md mandatory constraints | HIGH | LOW | P1 |
| Existing test patterns in CLAUDE.md | HIGH | LOW | P1 |
| Protocol reference docs (ezusb-iidx) | HIGH | HIGH | P2 |
| Per-bug research artifacts | HIGH | MEDIUM | P2 |
| Wine CI integration | MEDIUM | MEDIUM | P2 |
| Emulator state machine unit tests | HIGH | HIGH | P2 |
| Ghidra MCP integration | HIGH | HIGH | P3 |
| Static analysis in CI (cppcheck) | MEDIUM | MEDIUM | P3 |
| Game boot sequence expected logs | MEDIUM | MEDIUM | P3 |
| Headless Ghidra batch reference export | MEDIUM | MEDIUM | P3 |

**Priority key:**
- P1: Enables basic agentic work; must have for any meaningful agent productivity
- P2: Enables agent to address hardware protocol bugs; needed for the actual bug backlog
- P3: Advanced capability; high value once P1/P2 are stable

---

## Ecosystem Context

### What Exists in the Agentic RE Space (2025)

The agentic RE tooling ecosystem is active and maturing rapidly. Key tools relevant to this project:

**Ghidra MCP implementations:**
- [LaurieWired/GhidraMCP](https://github.com/LaurieWired/GhidraMCP) — original implementation; Ghidra plugin + Python MCP bridge; exposes decompilation, function listing, renaming, cross-reference queries
- [starsong-consulting/GhydraMCP](https://github.com/starsong-consulting/GhydraMCP) — multi-instance variant with REST API; 179 MCP tools covering batch operations, cross-binary documentation transfer
- [jtang613/GhidrAssistMCP](https://github.com/jtang613/GhidrAssistMCP) — 34 built-in tools; SSE and Streamable HTTP transports

**Headless Ghidra scripting:**
- [galoget/ghidra-headless-scripts](https://github.com/galoget/ghidra-headless-scripts) — batch decompilation/disassembly scripts
- [edmcman/GhidraFunctionCPPExporter](https://github.com/edmcman/GhidraFunctionCPPExporter) — exports decompiled C per-function with necessary type definitions

**Verdict for this project:** GhidraMCP (LaurieWired variant) is the right choice for interactive work. Headless Ghidra with custom Jython post-scripts is the right choice for batch reference export. The two are complementary and use the same Ghidra installation.

### What's Not Yet Standard

Automated agentic feedback loops for cross-compiled Windows DLL code on Linux are not well-documented patterns in 2025. The Wine + MinGW + Make combination is used by the project already, but the documentation for running it reliably in CI is thin. This project's `aide-verify.sh` + `Dockerfile.test` approach would be a reusable template for other RE projects with the same toolchain constraints.

---

## Sources

- Direct codebase analysis: `src/test/`, `src/main/`, `GNUmakefile`, `run-tests-wine.sh`, `.github/workflows/`, `Dockerfile.build` — HIGH confidence
- Existing research: `.planning/research/ARCHITECTURE.md`, `.planning/research/STACK.md`, `.planning/research/PITFALLS.md` — HIGH confidence (all derived from codebase)
- GhidraMCP ecosystem: [LaurieWired/GhidraMCP](https://github.com/LaurieWired/GhidraMCP), [starsong-consulting/GhydraMCP](https://github.com/starsong-consulting/GhydraMCP), [jtang613/GhidrAssistMCP](https://github.com/jtang613/GhidrAssistMCP) — MEDIUM confidence (WebSearch + GitHub)
- Ghidra headless scripting: [ghidra-headless-scripts](https://github.com/galoget/ghidra-headless-scripts), [GhidraFunctionCPPExporter](https://github.com/edmcman/GhidraFunctionCPPExporter), [Headless Analyzer README](https://static.grumpycoder.net/pixel/support/analyzeHeadlessREADME.html) — MEDIUM confidence (WebSearch + official docs)
- Context engineering for agentic coding: [Martin Fowler — Context Engineering for Coding Agents](https://martinfowler.com/articles/exploring-gen-ai/context-engineering-coding-agents.html), [Spotify Engineering — Feedback Loops for Background Coding Agents](https://engineering.atspotify.com/2025/12/feedback-loops-background-coding-agents-part-3) — MEDIUM confidence
- Wine + MinGW cross-compilation practices: [Cross-building GROMACS with MinGW and Wine (2025)](https://group.miletic.net/en/blog/2025-03-16-cross-building-and-cross-testing-gromacs-for-windows-on-linux-with-mingw-and-wine/) — MEDIUM confidence

---

*Feature research for: AI-agent-friendly development infrastructure on a reverse engineering codebase (Bemanitools)*
*Researched: 2026-03-01*
