# Stack Research

**Domain:** AI-assisted development infrastructure for C/Win32 DLL injection codebase
**Researched:** 2026-03-01
**Confidence:** MEDIUM (most tooling well-established; RE tooling integration with agentic workflow is newer territory)

## Context

This research covers AIDE tooling only — what to add to Bemanitools to enable agentic coding. The existing stack (MinGW cross-compilation, GNUmake, Docker, Wine) stays in place. The question is what to layer on top for AI-assisted development workflows.

Key constraints that filter every decision:

- Build system is GNU Make + MinGW cross-compilation; test binaries are Windows PE executables run under Wine
- No CMake; adding CMake as a build system requirement is out of scope
- Code is C99; `clang-format` is already enforced
- CI is GitHub Actions on `ubuntu-22.04`, building via Docker container
- Target binaries are proprietary Windows DLLs — cannot be checked in; only headers and protocol docs are feasible

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| Existing custom test framework | In-tree | Unit tests for pure C logic | Already integrated in GNUmake build; adding a third-party framework would require CMake or separate build plumbing that doesn't exist |
| cppcheck | 2.19+ | Static analysis on Linux host | Runs natively on the Linux host against C source files without needing to actually cross-compile; no CMake required; direct invocation against source directories works without a compilation database |
| Ghidra (headless) | 11.x | Decompilation of target DLLs for reference material | Free/OSS, NSA-maintained, headless CLI mode enables scriptable batch analysis; Python/Jython scripting for JSON export of function names, signatures, cross-references; strong Windows PE support |
| GhidraMCP | latest | Expose Ghidra analysis to Claude/AI agents via MCP | Multiple implementations exist (LaurieWired/GhidraMCP, starsong-consulting/GhydraMCP); connects Ghidra's analysis directly to MCP clients like Claude Code without manual scripting |
| Wine | system | Run cross-compiled test binaries on Linux | Already used by project (`run-tests-wine.sh`); the existing test runner leverages Wine; extend this for CI |
| GitHub Actions | — | CI: build and run tests on every PR | Already in place for master/tag builds; needs test job added |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| cmocka | 2.0.1 | Mock objects for C tests | Only if tests need to mock hardware boundaries (e.g., win32 API calls, serial port reads); the existing `*emu` modules are the current mocking strategy and may suffice |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| cppcheck | Static analysis for C code | Run on host (Linux) against source tree; use `--enable=all --suppress=missingIncludeSystem` to suppress unavoidable Windows header issues; no compilation database needed for basic checks |
| clang-format | Code formatting (already in use) | Already enforced via `make code-format`; already in `Dockerfile.dev` |
| Ghidra headless scripts | Batch decompile target DLLs to produce reference headers | Run `analyzeHeadless` against a Ghidra project containing the proprietary DLLs; post-scripts export function signatures, exports, cross-references as JSON; output committed as reference docs |
| CLAUDE.md / `.planning/` | Agentic context management | The primary mechanism for giving an AI agent project context; maps architecture, APIs, test patterns, and build commands so the agent can work without constant re-explanation |

---

## Installation

```bash
# cppcheck (on Debian/Ubuntu host — not inside Docker build container)
sudo apt-get install cppcheck

# Ghidra — download from official releases, not via apt
# https://github.com/NationalSecurityAgency/ghidra/releases
# Requires JDK 21+
sudo apt-get install default-jdk
# Then: unzip ghidra_11.x_PUBLIC.zip to /opt/ghidra

# GhidraMCP (choose one)
# LaurieWired variant (Ghidra plugin + Python MCP bridge):
# https://github.com/LaurieWired/GhidraMCP/releases
# starsong-consulting variant (REST API + MCP bridge, multi-instance):
# https://github.com/starsong-consulting/GhydraMCP

# cmocka (only if needed for mocking)
# Option A: apt
sudo apt-get install libcmocka-dev libcmocka0
# Option B: build from source with MinGW toolchain (for cross-compiled tests)
# Requires CMake toolchain file targeting i686-w64-mingw32 — non-trivial
```

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| cppcheck | clang-tidy | If the project migrated to CMake (clang-tidy needs a compilation database or CMake integration; running it against MinGW-targeted code on a Linux host produces thousands of false positives from MinGW headers) |
| cppcheck | Coverity / PVS-Studio | If the project needs enterprise-grade analysis with fewer false positives; both support MinGW cross-compile via compilation databases, but are commercial |
| Ghidra headless | IDA Pro headless | IDA has better analysis quality, but costs $3,500+/seat and has no free headless mode; Ghidra headless is free and scriptable |
| Ghidra headless | Binary Ninja | Better Python API and more modern UX; $299/seat for personal or free non-commercial headless license; viable if Ghidra analysis quality is insufficient |
| Existing custom test macros | cmocka | If tests start needing mock objects for system calls (Win32 API stubs); current `*emu` modules handle this at a higher level |
| Existing custom test macros | Unity (ThrowTheSwitch) | Unity is more popular in embedded C contexts and simpler than cmocka; but the in-tree framework already has the same functionality for this codebase |
| Wine (test runner) | Windows VM via GitHub Actions | GitHub Actions `windows-latest` runners can run the built `.exe` files natively; faster and avoids Wine compatibility edge cases; downside: much slower (Windows runners are slower) and requires uploading artifacts between jobs |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| clang-tidy (directly on cross-compile sources) | MinGW system headers cause thousands of false positives when clang-tidy processes them on a Linux host; the workaround (filtering system headers) is fragile and misses real errors hiding in the noise | cppcheck, which is designed to tolerate missing/mismatched headers |
| CMake for test infrastructure | The build system is GNU Make; introducing CMake creates two parallel build systems, diverges conventions, and creates confusion about which system is authoritative | Extend existing GNUmakefile and Module.mk patterns |
| Full decompilation as checked-in source | Legal risk (proprietary game binaries) and maintenance burden; decompiled pseudo-C is not the actual source | Commit curated reference files: exported function signatures, protocol documentation, and API contracts extracted from decompilation |
| Google Test / Catch2 | Both are C++ frameworks; the codebase is C99 (some C++ exists but tests are pure C) | Existing test framework or cmocka |
| ASAN/UBSAN (cross-compiled) | AddressSanitizer and UBSanitizer don't work reliably with MinGW cross-compilation targeting Windows; they require runtime support libraries that are Windows-specific | cppcheck for static analysis; Wine-run tests catch obvious memory corruption |
| Radare2/r2 | Steeper learning curve than Ghidra; scripting in r2lang is less well-documented for batch extraction; community size smaller | Ghidra headless with Python/Jython |

---

## Stack Patterns by Use Case

**Testing pure C logic (protocol parsers, config parsers, security IDs):**
- Use existing test framework (TEST_MODULE_BEGIN/TEST_MODULE_TEST macros)
- Compile as Windows PE, run under Wine via `make run-tests`
- No changes needed; extend the existing pattern

**Testing code that calls Win32 APIs or hardware protocols:**
- Write a stub/emu module (follow existing `bio2emu`, `acioemu` pattern)
- Link test against stub module instead of driver module
- If function pointer injection is needed: add a callback parameter to the function under test (existing pattern: `eam_io_init()` takes thread callbacks)

**Adding static analysis to CI:**
- Run `cppcheck src/main src/test --enable=all --suppress=missingIncludeSystem -j4` as a GitHub Actions step
- No compilation database needed for basic checks
- Add `--platform=win32A` or `--platform=win64` to improve Windows-specific checks

**Adding Wine test execution to CI:**
- The existing `run-tests-wine.sh` script already handles test execution
- Add `sudo apt-get install wine` to the GitHub Actions workflow
- Run `make run-tests` after `make build` in the CI job

**Providing RE reference material to AI agents:**
- Run Ghidra headless against proprietary DLLs (kept off-repo, referenced locally)
- Export function names, signatures, and pseudocode via Python post-scripts
- Commit extracted reference files to `.planning/references/` or `doc/protocol/`
- Include these files in AI agent context via CLAUDE.md `@` references

**Connecting Ghidra to Claude Code interactively:**
- Install GhidraMCP plugin in Ghidra (LaurieWired or GhydraMCP variant)
- Configure MCP client (Claude Code) to connect to Ghidra's HTTP endpoint
- Agent can then query decompiled functions, rename symbols, trace cross-references without manual export

---

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| Ghidra 11.x | JDK 21+ | JDK 17 no longer supported as of Ghidra 11; `sudo apt-get install default-jdk` on Ubuntu 24.04 gives JDK 21 |
| cmocka 2.0.x | C99 required | cmocka 2.0 upgraded requirement from C89 to C99; Bemanitools is already C99 so no conflict |
| GhidraMCP (LaurieWired) | Ghidra 10.x/11.x | Check release notes; 11.x requires the matching plugin version |
| Wine (ubuntu-22.04) | Wine 7.x | Ubuntu 22.04 ships Wine 7.0; sufficient for running MinGW-built test executables; game-level testing requires newer Wine or Windows VM |
| cppcheck 2.19+ | GCC 5.1+ / Clang 3.5+ minimum | Debian 11 apt ships older cppcheck; use PPA or build from source for 2.19 on ubuntu-22.04 |

---

## Sources

- cmocka.org — MinGW support confirmed, version 2.0.1 released 2025-12-19, C99 required [MEDIUM confidence — WebSearch verified against official site]
- https://github.com/LaurieWired/GhidraMCP — MCP server for Ghidra, active project [MEDIUM confidence — WebSearch + GitHub]
- https://github.com/starsong-consulting/GhydraMCP — Multi-instance Ghidra MCP with REST API [MEDIUM confidence — WebSearch]
- https://ghidra.re/ghidra_docs/api/ghidra/app/util/headless/AnalyzeHeadless.html — Official Ghidra headless docs [HIGH confidence — official docs]
- https://github.com/galoget/ghidra-headless-scripts — Community headless scripts for batch decompilation [LOW confidence — community project]
- https://discourse.llvm.org/t/issues-with-system-headers-when-using-clang-tidy-with-a-gcc-mingw64-compiler/87511 — Documented clang-tidy + MinGW header problem [MEDIUM confidence — LLVM forums]
- https://cppcheck.sourceforge.io/manual.html — cppcheck --platform flag and MinGW support [HIGH confidence — official docs]
- Existing project files (GNUmakefile, Dockerfile.build, run-tests-wine.sh, .github/workflows/) — authoritative source for current build/CI setup [HIGH confidence — code]

---
*Stack research for: Bemanitools AIDE (AI-Assisted Development Environment)*
*Researched: 2026-03-01*
