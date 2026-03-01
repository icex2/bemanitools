# Pitfalls Research

**Domain:** AI-assisted development infrastructure for a C/Win32 DLL injection and reverse engineering toolkit
**Researched:** 2026-03-01
**Confidence:** HIGH (codebase-grounded) / MEDIUM (agentic RE workflow patterns)

---

## Critical Pitfalls

### Pitfall 1: Testing Hardware-Protocol Code Without Real Hardware

**What goes wrong:**
Tests are written against the emulated device implementations (bio2emu, acioemu, ezusb-iidx-emu) rather than validating that the emulators themselves are correct. A test passing against `bio2emu` tells you nothing about whether `bio2emu` faithfully reproduces real BIO2 board behavior. The entire emulator can be wrong, and all tests pass.

**Why it happens:**
The real hardware (BIO2 boards, EZUSB boards, ACIO devices) is not available in CI. Developers reach for what they can test — the emulators — and treat a passing emulator test as equivalent to hardware validation. This confuses "tests the stub" with "tests the protocol."

**How to avoid:**
Treat the emulator implementations as the boundary, not the unit under test. Tests for emulated device protocols should test that the emulator produces wire-correct output against a known good protocol transcript (byte-level capture from real hardware). Record actual hardware traces as fixtures; replay them in tests. New emulator behavior requires a new trace as evidence.

**Warning signs:**
- Tests exist for `acioemu`, `bio2emu`, etc. but no fixtures capture real hardware packet sequences
- Bug reports where the game boots in CI but fails on real hardware (or vice versa)
- Emulator code changes go in without any hardware re-validation step in the PR checklist

**Phase to address:** Testing Infrastructure — establish protocol trace fixture format and at least one captured trace per emulated device before writing new emulator tests.

---

### Pitfall 2: CI Builds But Never Runs Tests

**What goes wrong:**
The current CI workflow (`build-master.yaml`) runs `make build-docker`, which compiles the code, but does not invoke `make run-tests`. Tests exist and pass locally via `run-tests-wine.sh`, but CI never executes them. A regression in any tested module goes undetected until a human runs tests manually.

**Why it happens:**
Running Wine in GitHub Actions requires explicit setup (Wine installation, `WINEARCH`, `DISPLAY` or headless configuration). The Docker build image (`Dockerfile.build`) does not include Wine. Adding Wine to a build container that already works is a separate concern that gets deferred indefinitely.

**How to avoid:**
Create a second Docker image (`Dockerfile.test`) that extends the build image with Wine and `xvfb` (headless display), or install Wine directly in the CI job before calling `make run-tests`. The Debian 11 `wine` package is available on `ubuntu-22.04` runners without custom repositories. Confirm that `WINEARCH=win32` and `WINEPREFIX` are set consistently with the local test runner.

**Warning signs:**
- `run-tests` target not referenced in any GitHub Actions workflow
- CI job takes under 60 seconds (no Wine initialization time)
- PRs landing with failing tests discovered only after merge

**Phase to address:** CI Pipeline — the very first phase; blocking for everything else.

---

### Pitfall 3: Wine Version Drift Between Local and CI Environments

**What goes wrong:**
The Wine version installed on a developer's machine differs from what CI installs. Some Windows API behaviors differ between Wine 5.x, 6.x, 7.x, and 9.x. A test that passes locally with Wine 9.x may fail in CI on Wine 5.0.3 (Debian 11 stable) because a Wine bug or a missing API stub is present in the older version. The opposite also occurs: Wine 5.x tolerates behavior that Wine 9.x correctly rejects.

**Why it happens:**
Debian's Wine packaging lags upstream by 6-18 months. The CI runner uses whatever `apt-get install wine` provides for the runner's OS. Developers on Arch or using WineHQ's staging PPA get a much newer build. The discrepancy is invisible until a test behaves differently.

**How to avoid:**
Pin a specific Wine version in the CI workflow and document it. Either use WineHQ's PPA in the GitHub Actions job to install a specific version, or include Wine in a versioned Docker test image. The test image approach (Dockerfile.test with a pinned Wine package) is more reproducible than PPA installation. Check `wine --version` output in CI and fail the job if it doesn't match the pinned version.

**Warning signs:**
- CI workflow uses `apt-get install wine` without a version pin
- Test failures that only reproduce on CI, not locally (or vice versa)
- The `run-tests-wine.sh` script has no `wine --version` check

**Phase to address:** CI Pipeline — document and pin Wine version in the first CI phase.

---

### Pitfall 4: AI Agent Generates Win32 Code With Wrong Calling Conventions

**What goes wrong:**
An AI agent generates or modifies C code that interacts with Win32 APIs, COM interfaces, or function pointers from hook tables and uses the wrong calling convention. The most common mistake: using `__cdecl` where `__stdcall` is required, or generating a hook function that has the right signature but the wrong `WINAPI` annotation. The resulting code compiles without warnings under MinGW (since the mismatch isn't always detected at compile time) but corrupts the stack at runtime, causing crashes or silent incorrect behavior that only manifests during game execution.

**Why it happens:**
LLMs are trained predominantly on x86-64 Linux code where calling conventions are irrelevant (System V AMD64 ABI is universal). Win32's `__stdcall` (callee cleans stack) is a 32-bit-era concern that rarely appears in modern training data. When the agent sees `BOOL WINAPI SomeFunction(HANDLE h)` it may not understand that `WINAPI` expands to `__stdcall` and is load-bearing. Decompiled code from Ghidra or IDA often drops or mistranslates calling convention annotations.

**How to avoid:**
Include Win32 calling convention rules explicitly in AI agent context: "All Win32 API functions and COM interface methods use `__stdcall` (annotated `WINAPI`). Function pointers in hook tables must match the original API's calling convention exactly. Do not omit `WINAPI` from hook implementations." Provide example hook function signatures from the codebase as context. Review any agent-generated hook code for calling convention correctness before building.

**Warning signs:**
- Agent-generated code omits `WINAPI` on new hook functions
- Agent adds `cdecl` calls into hook tables or function pointer assignments
- Stack corruption crashes that happen on the first call through a new hook but not on startup

**Phase to address:** Agent Context Engineering — establish calling convention rules as mandatory agent context before any code generation on hook modules.

---

### Pitfall 5: AI Agent Conflates BT5 Architecture With BT6 Refactoring Plans

**What goes wrong:**
The codebase contains both BT5 (current, stable, in `src/main/`) and references to the BT6 refactoring effort (issue #305, PRs #290-297). An AI agent without explicit scope constraints may read BT6 design discussions, issue comments, or partially-merged BT6 code and apply BT6 architectural patterns to BT5 code being fixed. This produces code that is structurally incompatible with BT5 module boundaries, introduces BT6-style abstractions that don't exist yet in BT5, or breaks BT5 builds because it references symbols that only exist in the BT6 branch.

**Why it happens:**
The agent has no reliable way to distinguish BT5 from BT6 intent without explicit instruction. GitHub issue discussions, PR comments, and commit messages freely mix both. A fix for a BT5 bug that references a BT6 design discussion may lead the agent to implement the BT6 solution in the BT5 codebase.

**How to avoid:**
Scope agent sessions explicitly: "This session targets BT5 (`src/main/` as it currently exists). Do not introduce patterns, abstractions, or module boundaries from BT6. Any BT6-only code paths, headers, or modules are out of scope." Include the list of BT6-specific issue numbers as off-limits references. Before each agent session, provide the current BT5 module list as the authoritative structure.

**Warning signs:**
- Agent proposes creating new abstraction layers not present in BT5
- Agent references issues #290-297 or #305 in proposed fixes
- Agent-generated code introduces new header files that do not correspond to existing BT5 modules

**Phase to address:** Agent Context Engineering — establish BT5 scope constraints as part of agent system prompt setup.

---

### Pitfall 6: Decompiled Symbol Reference Material Is Treated as Ground Truth

**What goes wrong:**
Decompiled output from Ghidra or IDA Pro (used to understand Konami game binary internals) is imported into agent context or codebase documentation and treated as definitively correct C. The agent then generates hook code or protocol emulation based on decompiler output that contains errors: wrong types inferred for function arguments, incorrect struct layouts, missed pointer indirections, wrong array bounds, or `FUN_00401234`-style names that carry no semantic meaning. Hook code built on this incorrect reference compiles but produces wrong behavior against the actual game.

**Why it happens:**
Decompilers make educated guesses. Ghidra's output for MIPS or custom Konami RISC CPU code has known error modes: struct reconstruction fails without RTTI, array indexing is frequently misread as pointer arithmetic, and `void*` is common where the actual type matters. Developers and agents treat pseudo-C as authoritative because it looks like real code.

**How to avoid:**
Mark all decompiler-derived information explicitly as hypothesis, not fact. Create a convention: decompiled analysis goes in a `docs/reversing/` directory with a header "UNVERIFIED DECOMPILER OUTPUT — validate against hardware behavior before using as implementation reference." Agent context should include: "Decompiled pseudo-C in docs/reversing/ is advisory only. Protocol behavior must be validated against actual hardware traces or game log output."

**Warning signs:**
- Decompiled C pasted directly into comments without any "decompiler output" caveat
- Agent references `FUN_` symbols from Ghidra output as if they are known API names
- Hook implementations reference struct field offsets from decompiler output without a hardware-verified test

**Phase to address:** Reference Material Creation — establish the "hypothesis, not fact" convention before any decompiled code enters the documentation or agent context.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Test only the emulator, not the protocol | Fast test iteration, no hardware needed | Silent emulation correctness bugs; game works in CI, fails on real cabinet | Never — always need at least one protocol trace fixture |
| Skip Wine in CI to keep builds fast | CI runs in under 60 seconds | Regressions in tested modules ship undetected | Never — Wine overhead is ~30s per test binary, acceptable |
| Use `wine` from distro packages without pinning | Zero CI setup effort | Test results differ across developer environments | Only acceptable during initial scaffolding; pin before first milestone |
| Agent generates code from decompiler output without review | Faster research-to-code turnaround | Silent behavioral bugs in hooks and emulators | Never for protocol emulation code; acceptable for utility/config code |
| Hard-code game binary offsets in comments without versioning | Documents a discovery quickly | Offsets change per game version; unversioned comments mislead future maintainers | Never — always tag offsets with game version and binary hash |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Wine test runner in CI | Installing Wine via `apt` without `WINEARCH=win32` set, causing 64-bit Wine to attempt 32-bit executables and fail with opaque errors | Set `WINEARCH=win32`, `WINEPREFIX=/tmp/wine`, and `DISPLAY=` (or use `xvfb-run`) before invoking test binaries |
| MinGW 32-bit cross-compiler | Using `x86_64-w64-mingw32-gcc` (64-bit) for modules that must be 32-bit DLLs | The build system already handles this via `indep-32`/`indep-64` targets; new modules must declare the correct bitness in `Module.mk` |
| Ghidra MCP / RE assistant tools | Feeding full decompiled output into agent context | Provide only the specific function being investigated; full decompiler dumps exhaust context windows and increase hallucination rate |
| GitHub Actions artifact upload | Uploading the dist `.zip` directly (current approach uses v4 which works, but prior v2 was broken — see commit history) | Always specify `actions/upload-artifact@v4` and test on a PR before landing changes |
| cconfig `.conf` parsing in tests | Writing tests that depend on config file paths relative to the test binary's working directory | Use absolute paths or pass config content as strings; Wine changes working directory conventions |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Loading all hook module source into agent context for every session | Agent context window saturated; quality of responses degrades; later instructions ignored | Load only the module being fixed; use a codebase map (file tree + module summaries) instead of full source | At ~30+ source files in context; context quality degrades noticeably |
| Running full Wine test suite for every CI push | CI takes 10+ minutes for trivial changes; developers bypass CI | Separate fast (build-only) and slow (build + test) CI stages; run tests only on main and PRs | Not a scale problem; primarily a DX problem that causes CI to be ignored |
| Generating a new agent session per bug fix without preserving context | Agent re-reads the same architecture files repeatedly; context fills with repeated groundwork | Maintain session-level architecture summaries; include them as compact context at session start | Immediate — each session without a pre-loaded summary wastes 20-30% of context window |

---

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Committing real game binary hashes or offsets from copyrighted games | DMCA exposure; the repository becomes a tool for identifying specific game versions | Document protocol behavior only; reference hardware traces, not game binary internals; review agent output for binary-derived constants before commit |
| Including proprietary protocol documentation verbatim in agent context | Legal ambiguity around reproduction of proprietary specs | Use derived documentation only (observed wire behavior, not copied from Konami docs); mark all protocol docs as "observed behavior" |
| AI agent generating code that disables or bypasses the `security/` module | Would allow boot without PCBID validation, potentially enabling piracy | Explicitly include in agent constraints: "Do not modify security/ module logic; do not add code that bypasses PCBID or security dongle checks" |

---

## "Looks Done But Isn't" Checklist

- [ ] **CI runs tests:** Verify `run-tests` is called in the workflow, not just `build` — check the actual YAML, not the README
- [ ] **Wine version is pinned:** `apt-get install wine` without a version pin means the version drifts silently — check for a specific pinned version or Dockerfile hash
- [ ] **Protocol test has a hardware trace fixture:** A test for an emulated device that has no reference to a real hardware capture is testing the stub, not the protocol
- [ ] **Agent context includes calling convention rules:** If an agent session modifies any hook or function pointer code, verify `__stdcall`/`WINAPI` rules were in the system prompt
- [ ] **BT5 scope is explicit in agent sessions:** Any session touching bug fixes must have an explicit "BT5 only, not BT6" constraint — check session setup, not agent output
- [ ] **Decompiled code is marked as hypothesis:** Any file derived from Ghidra/IDA output must have a "UNVERIFIED DECOMPILER OUTPUT" header before it enters agent context

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| CI builds without running tests for months | MEDIUM | Add Wine to CI image, run tests, triage failures; most will be pre-existing issues that need to be marked expected or fixed |
| Agent generated hooks with wrong calling convention | MEDIUM | Check all agent-generated hook files for `WINAPI`/`__stdcall`; compile with `-Wcast-function-type` and `-Wbad-function-cast`; run existing tests under Wine |
| Decompiled code treated as ground truth in documentation | LOW | Add "UNVERIFIED" header to all existing reversing docs; audit agent-generated code that references those docs |
| Wine version drift causing test discrepancy | LOW | Pin version in Dockerfile; rebuild CI image; rerun test suite; investigate any new failures against Wine changelog |
| BT6 abstractions introduced into BT5 | HIGH | Revert all agent-generated files touching affected modules; re-scope session with explicit BT5 constraints; re-implement narrowly |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| CI builds but never runs tests | Phase 1: CI Pipeline | Workflow YAML references `run-tests`; CI job log shows Wine test output |
| Wine version drift | Phase 1: CI Pipeline | CI job prints `wine --version`; output matches pinned version in Dockerfile |
| Hardware protocol testing gap | Phase 2: Testing Infrastructure | At least one protocol trace fixture per emulated device exists in `src/test/` |
| Agent generates wrong calling conventions | Phase 3: Agent Context Engineering | Agent system prompt includes calling convention rules; PR checklist requires hook review |
| Agent conflates BT5 and BT6 | Phase 3: Agent Context Engineering | Session setup doc explicitly scopes BT5; BT6 issue numbers listed as off-limits references |
| Decompiled code as ground truth | Phase 4: Reference Material Creation | All files in `docs/reversing/` have "UNVERIFIED DECOMPILER OUTPUT" header; agent context includes advisory-only caveat |
| Context window saturation | Phase 3: Agent Context Engineering | Agent sessions start with compact architecture summary, not full source dumps; session logs show context utilization below 60% |

---

## Sources

- Codebase analysis: `.planning/codebase/TESTING.md`, `.planning/codebase/CONCERNS.md`, `.planning/codebase/ARCHITECTURE.md` (HIGH confidence — direct codebase observation)
- CI configuration: `.github/workflows/build-master.yaml`, `Dockerfile.build`, `run-tests-wine.sh` (HIGH confidence — direct codebase observation)
- MinGW/Wine cross-compilation pitfalls: [Cross-building GROMACS with MinGW and Wine (2025)](https://group.miletic.net/en/blog/2025-03-16-cross-building-and-cross-testing-gromacs-for-windows-on-linux-with-mingw-and-wine/), [Cross compiling Windows binaries from Linux](https://jake-shadle.github.io/xwin/) (MEDIUM confidence)
- Win32 calling convention bugs: [Win32 calling conventions: Usage cases](http://www.nynaeve.net/?p=42), [x86 calling conventions — Wikipedia](https://en.wikipedia.org/wiki/X86_calling_conventions) (HIGH confidence — well-documented ABI fact)
- LLM hallucination in code generation: [LLM Hallucinations in Practical Code Generation (ACM 2025)](https://dl.acm.org/doi/pdf/10.1145/3728894), [LLM Hallucinations in AI Code Review](https://diffray.ai/blog/llm-hallucinations-code-review/) (MEDIUM confidence)
- Decompiler output limitations: [LLM4Decompile](https://github.com/albertan017/LLM4Decompile), [reverser_ai](https://github.com/mrphrazer/reverser_ai), [AI for RE (reveng.ai blog)](https://blog.reveng.ai/training-an-llm-to-decompile-assembly-code/) (MEDIUM confidence)
- Agent context management: [Context Engineering for Coding Agents (Martin Fowler)](https://martinfowler.com/articles/exploring-gen-ai/context-engineering-coding-agents.html), [Taming Context Windows](https://www.agentic-engineer.com/blog/2025-10-28-taming-context-windows) (MEDIUM confidence)

---

*Pitfalls research for: AI-assisted development infrastructure on a C/Win32 DLL injection and reverse engineering codebase (Bemanitools)*
*Researched: 2026-03-01*
