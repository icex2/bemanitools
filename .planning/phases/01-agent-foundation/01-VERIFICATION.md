---
phase: 01-agent-foundation
verified: 2026-03-01T12:30:00Z
status: human_needed
score: 4/5 must-haves verified
human_verification:
  - test: "Run scripts/aide-verify.sh end-to-end in a clean environment with Wine installed"
    expected: "[BUILD] PASS, [FORMAT] PASS, [TEST] PASS — all existing unit tests pass under Wine"
    why_human: "Cannot run Wine tests or a full build in this environment; test pass/fail requires actual Wine execution of the .exe files"
---

# Phase 1: Agent Foundation Verification Report

**Phase Goal:** Agents can build, test, and verify changes without constant human tooling intervention
**Verified:** 2026-03-01T12:30:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Running `scripts/aide-verify.sh` produces structured `[BUILD]`/`[FORMAT]`/`[TEST]` PASS/FAIL output an agent can parse without reading GNUmakefile | VERIFIED | Script exists, is executable, has `set -uo pipefail`, each stage prints tagged line last, `grep '[BUILD]'` works |
| 2 | CI runs Wine tests on every push and fails the build on test regressions | VERIFIED | `build-master.yaml` has "Run tests (Wine)" step invoking `run-tests-wine.sh` inside `bemanitools-build:latest`; step has no `continue-on-error`; image name matches `docker_build_image_name` in GNUmakefile |
| 3 | An agent starting a fresh session can orient to the codebase by reading a single reference file | VERIFIED | `doc/agent-reference.md` exists with Tier 1/2/3 module map, test coverage table, build quick-start, and hook architecture overview; CLAUDE.md points to it directly |
| 4 | CLAUDE.md constrains agents to BT5 scope, Win32 calling conventions, and correct test patterns | PARTIAL | CLAUDE.md enforces BT5 scope, Windows-only target, `check.h` test patterns, and do-not-touch list. Win32 "calling conventions" (e.g., `__stdcall`/`WINAPI`) are not explicitly addressed — only the Win32 target platform is stated. Functional gap is minor: no BT code is written yet, so no incorrect calling convention has been introduced. |
| 5 | Existing unit tests can be run under Wine and all pass | HUMAN NEEDED | Test runner (`dist/test/run-tests.sh`) uses `set -e` and invokes 17 Wine test executables. Infrastructure is wired correctly. Whether tests actually pass requires running Wine. |

**Score:** 4/5 truths verified (1 partial, 1 human-needed)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `scripts/aide-verify.sh` | Three-stage verification loop with tagged output | VERIFIED | 46 lines, executable (`-rwxr-xr-x`), bash syntax clean, `[BUILD]`/`[FORMAT]`/`[TEST]` tags present, `git diff --exit-code` for format check, `exit 1` on each failure |
| `Dockerfile.build` | Build container with Wine for test execution | VERIFIED | `dpkg --add-architecture i386` added, `wine` and `wine32` in apt-get install list |
| `.github/workflows/build-master.yaml` | CI pipeline with Wine test execution step | VERIFIED | "Run tests (Wine)" step runs after "Build", uses `bemanitools-build:latest` image (same tag GNUmakefile produces), no `continue-on-error` |
| `doc/agent-reference.md` | Agent-oriented codebase reference with module tiers and test coverage | VERIFIED | 124 lines, Tier 1/2/3 module map, test coverage table, hook architecture diagram, build quick-start table, links to `doc/development.md` and `doc/architecture.md` (both exist) |
| `CLAUDE.md` | Project-level agent constraints and conventions | VERIFIED | 51 lines, BT5 scope, verification workflow, coding conventions, do-not-touch list, pointer to `doc/agent-reference.md` |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `scripts/aide-verify.sh` | `GNUmakefile` | `make build`, `make code-format`, `make run-tests` | WIRED | All three make targets verified to exist in GNUmakefile (lines 64, 78, 86) |
| `scripts/aide-verify.sh` | `run-tests-wine.sh` | `make run-tests` invokes it | WIRED | GNUmakefile `run-tests` target at line 86 runs `./run-tests-wine.sh`; `run-tests-wine.sh` exists and calls `dist/test/run-tests.sh` via `./run-tests.sh` after unzip |
| `.github/workflows/build-master.yaml` | `run-tests-wine.sh` | CI step runs it inside Docker container | WIRED | Step runs `docker run ... /bin/sh -c "cd /bemanitools && ./run-tests-wine.sh"` |
| `Dockerfile.build` | `run-tests-wine.sh` | Wine packages enable test execution | WIRED | `wine` and `wine32` installed in image; `run-tests-wine.sh` calls `wine ./...exe` for each test |
| `CLAUDE.md` | `doc/agent-reference.md` | Points agents to reference doc | WIRED | Line 5: `see [doc/agent-reference.md](doc/agent-reference.md)` |
| `doc/agent-reference.md` | `scripts/aide-verify.sh` | Quick-start section | WIRED | Lines 8 and 24 both reference `scripts/aide-verify.sh` |
| `doc/agent-reference.md` | `doc/development.md` | Links for depth | WIRED | Lines 13 and 98; `doc/development.md` exists |
| `doc/agent-reference.md` | `doc/architecture.md` | Links for depth | WIRED | Line 82; `doc/architecture.md` exists |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| AIDE-01 | 01-01-PLAN.md | Build system works reliably for agentic iteration | SATISFIED | `aide-verify.sh` wraps `make build` with grep-parseable output; agents do not need to read GNUmakefile |
| AIDE-02 | 01-01-PLAN.md | Testing infrastructure established where feasible | SATISFIED | Wine added to Dockerfile, CI step runs tests, `make run-tests` wired through to Wine-executed test binaries |
| AIDE-04 | 01-02-PLAN.md | Development workflow documented for agentic use | SATISFIED | `CLAUDE.md` and `doc/agent-reference.md` cover build, format, test, module tiers, conventions, and do-not-touch list |

No orphaned requirements: REQUIREMENTS.md maps AIDE-01, AIDE-02, and AIDE-04 to Phase 1. All three are claimed by plans and verified.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `doc/agent-reference.md` | 51, 75 | States `iidxhook8` "has automated test: `iidxhook8-test`" but `dist/test/run-tests.sh` does not invoke it | Info | Misleading: agents reading the reference will expect iidxhook8 tests to run via `make run-tests`, but they do not. The test source and Module.mk exist (`src/test/iidxhook8/`) but the runner does not include them. Not a goal blocker — the phase does not require iidxhook8 tests to run. |

No placeholder implementations, empty handlers, or stub API routes found. All three phase artifacts contain substantive, working content.

### Human Verification Required

#### 1. Existing Unit Tests Pass Under Wine

**Test:** In an environment with Wine installed, run `scripts/aide-verify.sh` from the repository root after a successful build.

**Expected:** All three stages print PASS. Specifically: 17 Wine test invocations in `dist/test/run-tests.sh` (cconfig, iidxhook-util, security, util-net, d3d9hook) all exit 0, and the script prints `[TEST] PASS`.

**Why human:** Cannot run Wine or `make build` in the current environment. The infrastructure is correctly wired — `run-tests-wine.sh` calls `run-tests.sh` which uses `set -e` and exits non-zero on failure — but whether the compiled test binaries actually pass requires execution.

### Gaps Summary

No blocking gaps. The phase goal is structurally achieved: the verification script exists and is machine-parseable, CI is wired to catch test regressions, the agent reference doc provides a complete orientation, and CLAUDE.md enforces scope and conventions.

One minor discrepancy: the success criterion specifies CLAUDE.md should constrain "Win32 calling conventions" explicitly, but CLAUDE.md only states the Win32 target platform (not `__stdcall`/`WINAPI` calling annotation rules). This is not a functional gap at phase 1 — no hook code is being written yet. If calling convention correctness becomes relevant in later phases (e.g., when fixing hook bugs), CLAUDE.md should be updated to address it explicitly.

One informational discrepancy: `doc/agent-reference.md` lists `iidxhook8-test` as running under the test suite, but `dist/test/run-tests.sh` does not invoke it. The source and Module.mk exist but the runner is incomplete. This does not block Phase 1 goals.

---

_Verified: 2026-03-01T12:30:00Z_
_Verifier: Claude (gsd-verifier)_
