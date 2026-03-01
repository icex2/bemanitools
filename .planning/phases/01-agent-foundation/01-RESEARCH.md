# Phase 1: Agent Foundation - Research

**Researched:** 2026-03-01
**Domain:** Shell scripting, CI/CD (GitHub Actions), documentation, project CLAUDE.md
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Verification script (aide-verify.sh):**
- Three stages in fixed order: build, code-format check, tests — always all three, fail fast on first failure
- Tagged line output: `[BUILD] PASS`, `[FORMAT] FAIL`, `[TEST] PASS` — grep-friendly, matches roadmap success criteria
- Inline error output before each stage's tag so agents can diagnose without re-running
- No partial-run flags — simplicity over flexibility
- Lives in `scripts/aide-verify.sh`
- Uses existing make targets: `make build`, `make code-format` (diff check), `make run-tests`

**Codebase reference file (doc/agent-reference.md):**
- Lives in `doc/` alongside existing `architecture.md` and `development.md`
- Module map grouped by tier: pure logic (testable), emulation (partially testable), hooks (hardware-dependent)
- Test coverage map: which modules have tests, which don't
- Hook architecture overview: DLL injection, IAT patching, emulator tiers
- Build/verify quick-start pointing to `aide-verify.sh` and `doc/development.md`
- Links out to existing docs for depth — not self-contained, avoids duplication

**CLAUDE.md (project-level agent constraints):**
- Lives at repo root as `CLAUDE.md`
- Strict guardrails — explicitly state conventions, no room for agents to invent patterns
- Self-contained: no dependency on personal `~/.claude/rules/`, works for any contributor
- Scope constraints: BT5 only, no new dependencies, Windows-only target (Win32 via MinGW cross-compilation), test-before-commit
- Coding patterns: C99, check.h macros for tests, Module.mk for new code, clang-format before commit
- Do-not-touch list: GNUmakefile, root Module.mk, .clang-format, imports/
- Points to `doc/agent-reference.md` for codebase orientation

**CI pipeline:**
- Extend existing Docker build image (Dockerfile.build) with Wine for test execution
- Keep existing trigger: push to master only
- Add Wine test execution step after build step
- Test failures block the build (red pipeline)
- Keep Docker-based build approach (`make build-docker`)

### Claude's Discretion

- CI workflow structure: extend existing file vs new file
- Exact structure and section ordering within agent-reference.md
- CLAUDE.md tone and internal organization
- aide-verify.sh implementation details (error capture, color output, exit codes)
- Whether to consolidate run-tests-wine.sh into aide-verify.sh or keep it separate

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| AIDE-01 | Build system works reliably for agentic iteration (build, verify, repeat) | `aide-verify.sh` wraps existing make targets into a single structured command; CI validates build on every push |
| AIDE-02 | Testing infrastructure established where feasible (unit tests for pure logic, integration stubs) | Existing Wine test runner and `run-tests-wine.sh` functional; CI needs Wine added to Docker image; tests already compile and run |
| AIDE-04 | Development workflow documented for agentic use (how to build, test, verify changes) | `doc/agent-reference.md` + `CLAUDE.md` provides orientation and constraints; points agents at `aide-verify.sh` for verify loop |
</phase_requirements>

## Summary

Phase 1 is a pure infrastructure and documentation phase — no application logic changes. All four deliverables (aide-verify.sh, doc/agent-reference.md, CLAUDE.md, CI Wine integration) are additive: they create new files or extend existing ones without modifying production code.

The technical work is straightforward but requires precise knowledge of the existing codebase. The verify script must wrap existing make targets correctly, handle the `code-format` diff-check idiom, and produce machine-parseable tagged output. The CI change requires adding Wine to the Docker build image (Debian 11.6-slim) and running `run-tests-wine.sh` inside the container. The reference doc and CLAUDE.md are authoring tasks with clear scope from CONTEXT.md.

The primary risk is the `code-format` diff-check implementation: `make code-format` applies clang-format in place (mutates files), so a non-destructive diff-check needs a different approach. The correct pattern is: run `make code-format`, then check `git diff --exit-code` to detect whether any files were changed. This is the standard approach for in-place formatters with a diff-check CI mode.

**Primary recommendation:** Implement all four deliverables in a single atomic plan. The only non-trivial decision is the code-format diff-check approach — use `git diff --exit-code` after `make code-format` to detect formatting violations without permanently mutating files.

## Standard Stack

### Core

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| bash | System | aide-verify.sh implementation | Already used in run-tests-wine.sh; project standard for scripts |
| GitHub Actions | N/A | CI workflow | Existing CI is GH Actions; extend, don't replace |
| Docker | debian:11.6-slim | Build container base image | Current Dockerfile.build uses this exact base image |
| wine | 7.x (ubuntu-22.04) | Test execution in CI | Matches the STATE.md concern; ubuntu-22.04 ships Wine 7.x |
| clang-format | System | Format checking | Already used by `make code-format` target |
| git | System | Format diff detection | Available in all environments; used by build system already |

### Supporting

| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| make | System | Build orchestration | All build/test/format steps go through existing make targets |
| actions/checkout | v2 (existing) | CI checkout | Already used; match existing version for consistency |
| actions/upload-artifact | v4 (existing) | CI artifact upload | Already in workflow; reuse |

## Architecture Patterns

### Verification Script Structure

The script wraps three existing make targets and produces tagged output. Each stage captures stderr/stdout, emits inline errors, then prints the tagged result line. Fail-fast on first failure via `set -e` equivalent or explicit exit.

```bash
#!/bin/bash
set -euo pipefail

RESULT=0

run_stage() {
    local tag="$1"
    shift
    if "$@" 2>&1; then
        echo "[$tag] PASS"
    else
        echo "[$tag] FAIL"
        exit 1
    fi
}

run_stage "BUILD" make build
# Format check: apply in place, then check if git reports any diff
make code-format
if ! git diff --exit-code -- src/main src/test > /dev/null 2>&1; then
    git diff -- src/main src/test  # show diff inline for agent diagnosis
    echo "[FORMAT] FAIL"
    exit 1
fi
echo "[FORMAT] PASS"

run_stage "TEST" make run-tests
```

This is illustrative; exact implementation is discretionary. Key invariants:
- Tagged output lines are the last thing printed for each stage
- Errors appear before the tag, not after
- Non-zero exit on any failure

### Code-Format Diff Check

`make code-format` runs `clang-format -i` (in-place mutation). A diff-check requires:

1. Run `make code-format` (applies formatting)
2. Run `git diff --exit-code -- src/main src/test`
3. If diff is non-empty, formatting was not clean; print the diff and fail

This pattern works because git tracks the original file state. After the verify run, the working tree has formatted files — which is fine if the agent then commits. If an agent runs `aide-verify.sh` for read-only checking, it should `git stash` or `git checkout` the formatted changes afterward; the script itself should not do this (keep it simple).

Alternative: `clang-format --dry-run --Werror` is available in clang-format 10+. This avoids file mutation entirely. However, it requires knowing which version is installed on CI, and the project currently uses `make code-format` for formatting — using `--dry-run` would duplicate the file list logic. The `git diff` approach is simpler and more consistent with the existing make target.

**Recommendation:** Use the `git diff --exit-code` approach. It's simpler, works with any clang-format version, and reuses the existing `make code-format` target without duplication.

### CI Wine Integration

The existing Dockerfile.build (debian:11.6-slim) does not include Wine. Two approaches:

**Option A: Modify Dockerfile.build** — Add Wine to the build image. This means Wine is available in the same Docker container where the build runs. The `make run-tests` target calls `run-tests-wine.sh` which calls `wine`. Clean, simple, single container.

**Option B: Separate CI step with Wine on the runner** — After the Docker build step, run tests on the ubuntu-22.04 runner directly (Wine 7.x is available via `apt`), unpacking `build/tests.zip` (if it exists) and running `run-tests-wine.sh`.

**Option A is preferred** because:
- Tests run in the same environment as the build (reproducible)
- No coordination between host and container needed
- Consistent with `make release` which runs build + tests in one pass
- `run-tests-wine.sh` expects to be run from the repo root and accesses `build/tests.zip`

The concern about Wine version (ubuntu-22.04 ships Wine 7.x, STATE.md flags this) applies to both options. Adding Wine to the Debian 11 build image instead of relying on the ubuntu-22.04 runner's Wine gives explicit control over version.

**Recommendation:** Add Wine to Dockerfile.build. Then add a CI step that runs `make run-tests` (or `./run-tests-wine.sh` directly) after the existing build step, within the same Docker container invocation.

### Wine in Debian 11 Docker Image

Debian 11 (bullseye) packages Wine as `wine` and `wine32`/`wine64`. The 32-bit support requires enabling i386 architecture. The test executables are built for both 32-bit and 64-bit (see `dist/test/run-tests.sh` — all tests run as `wine ./test.exe` without explicit arch flag, so Wine autodetects).

```dockerfile
# Enable 32-bit architecture for Wine 32-bit support
RUN dpkg --add-architecture i386
RUN apt-get update && apt-get install -y --no-install-recommends \
    wine \
    wine32 \
    ...
```

Note: `wine` on Debian 11 is Wine 6.x (not 7.x). This is fine for running Windows test executables compiled with MinGW — the tests use standard Win32 APIs (stdio, string ops) with no hardware dependencies.

**Confidence:** MEDIUM (based on knowledge of Debian package versions; exact Wine version on Debian 11 should be verified during implementation by checking `apt-cache show wine` in container).

### CI Workflow Extension

The existing `.github/workflows/build-master.yaml` has a build job with these steps: checkout, install prerequisites, build (make build-docker), prepare artifact, upload artifact. Test execution should be added as a step between "Build" and "Prepare artifact package".

Two sub-options:
- **Extend existing build-master.yaml** with a new step — simpler, fewer files
- **New workflow file** — cleaner separation, easier to disable independently

**Recommendation:** Extend the existing `build-master.yaml`. Adding one step doesn't justify a new file, and it keeps the "build + test" pipeline visible as a unit.

The new step runs `./run-tests-wine.sh` after the Docker build has produced `build/tests.zip`. The `run-tests-wine.sh` script unpacks and runs, so it must be run from the repo root where `build/` exists.

However: The current Docker-based build (`make build-docker`) runs the build inside a container and mounts the repo directory. `build/tests.zip` is produced inside the container and available on the host after the step completes. A native runner step can then use Wine from apt (or from the container). Since Dockerfile.build doesn't include Wine yet, the cleanest path is:

1. Add Wine to Dockerfile.build
2. Modify the `make build-docker` entrypoint (or add a second docker run command in CI) to also run tests

OR (simpler and non-invasive to Dockerfile.build):

1. Keep Dockerfile.build unchanged
2. Add a native CI step: `sudo apt-get install -y wine32 && ./run-tests-wine.sh`

**Recommendation for CI:** Add a native runner step installing Wine from apt and running `./run-tests-wine.sh`. This avoids touching Dockerfile.build (which is also used for tag builds), keeps the separation clean, and unblocks the Wine version question — ubuntu-22.04 ships Wine development packages that are sufficient for running standard Win32 test executables.

This contradicts the CONTEXT.md decision ("Extend existing Docker build image with Wine"). The planner should follow the locked decision and add Wine to Dockerfile.build.

### Module Tier Classification for agent-reference.md

Based on STRUCTURE.md and ARCHITECTURE.md, modules fall into three tiers:

**Tier 1 — Pure logic (testable, no hardware):**
- `cconfig/` — Config file parsing; fully tested (`src/test/cconfig/`)
- `security/` — Konami security emulation; fully tested (`src/test/security/`)
- `util/` — Logging, memory, threading utilities; partially tested (`src/test/util/`)
- `iidxhook-util/` (config subset) — Config parsing helpers; tested (`src/test/iidxhook-util/`)

**Tier 2 — Emulation (partially testable via Wine):**
- `d3d9hook/` — D3D9 hooking (tested but incomplete, CONCERNS.md notes gap)
- `ezusb-emu/`, `ezusb-iidx-emu/` — ezusb message node state machines (no automated tests)
- `acioemu/`, `bio2emu/` — ACIO/BIO2 emulation (no automated tests)
- `iidxhook8/` — IIDX8-specific hook (test exists: `src/test/iidxhook8/`)

**Tier 3 — Hooks/hardware-dependent (manual testing only):**
- `iidxhook1-8/`, `ddrhook1-2/`, `jbhook1-3/`, `popnhook1/`, `sdvxhook/` — Game-specific hook DLLs
- `iidxio/`, `ddrio/`, `eamio/`, `jbio/`, `popnio/`, `sdvxio/` — I/O dispatcher DLLs
- `ezusb/`, `ezusb2/`, `bio2/`, `bio2drv/`, `acio/`, `aciodrv/` — Hardware drivers
- `hook/`, `hooklib/` — Hook infrastructure (Win32 PE manipulation)
- `inject/`, `launcher/` — Game launch utilities

This classification is HIGH confidence — derived directly from codebase analysis and `src/test/` directory contents.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Format diff detection | Custom file comparison | `git diff --exit-code` after `make code-format` | Git knows original; single command, no state |
| CI test steps | Separate test framework | `./run-tests-wine.sh` (existing) | Already works; proven; don't add a wrapper around a wrapper |
| Module documentation | Auto-generate from source | Hand-written reference with tier classification | Source lacks docstrings; hand-written is more accurate and agent-optimized |
| Wine test orchestration | New test runner | Existing `dist/test/run-tests.sh` + `run-tests-wine.sh` chain | Already handles unzip + wine invocation |

**Key insight:** Every building block already exists. This phase assembles them, it does not invent new mechanisms.

## Common Pitfalls

### Pitfall 1: code-format mutating files breaks subsequent git diff

**What goes wrong:** `aide-verify.sh` runs `make code-format` (in-place), then `git diff` shows formatted changes mixed with the agent's actual code changes, causing false FAIL on subsequent runs after auto-format.

**Why it happens:** `make code-format` uses `clang-format -i` which writes files. If code was already formatted, git diff is empty (PASS). If code was not formatted, git diff is non-empty (FAIL), and the script leaves modified files in working tree.

**How to avoid:** Document in `aide-verify.sh` header comments that FORMAT FAIL leaves formatted files in place — agents should inspect `git diff` output and commit the formatting fix before re-running. The script should not auto-stash/restore; keep it simple.

**Warning signs:** Repeated FORMAT FAIL even after "fixing" formatting — agent isn't committing the clang-format output.

### Pitfall 2: Wine test failure masking

**What goes wrong:** `run-tests-wine.sh` uses `set -e` via `chmod +x run-tests.sh && ./run-tests.sh` inside the unzipped tests directory. If Wine initialization fails silently (e.g., missing 32-bit libraries), Wine exits 0 but test output is absent.

**Why it happens:** Wine may not output to stderr on init failures in some configurations; the exit code from a Wine process that failed to load is not always 1.

**How to avoid:** In CI, check that test output contains "All tests successful." before marking the step passed. Alternatively, ensure `run-tests.sh` contains `set -e` (it does: `set -e` is present in `dist/test/run-tests.sh`). The key line `echo "All tests successful."` at the end is the sentinel.

**Warning signs:** CI test step exits 0 but no "All tests successful." in output.

### Pitfall 3: Docker entrypoint conflict for Wine tests

**What goes wrong:** If Wine is added to Dockerfile.build and the ENTRYPOINT is modified to also run tests, the `make build-docker` command used in CI will now run tests as part of the build — but `make build-docker` mounts the repo and the container runs from inside `/bemanitools`. The test zip unpacking in `run-tests-wine.sh` uses relative path `build/tests.zip` which works from repo root.

**Why it happens:** The existing ENTRYPOINT is `cd /bemanitools && make`. Adding `&& make run-tests` to this ENTRYPOINT means the CI step `make build-docker` handles both. But if Wine fails in the container, the whole `make build-docker` exits non-zero and CI fails at "Build" not "Test" — hard to distinguish.

**How to avoid:** Keep tests as a separate CI step (not embedded in Dockerfile.build ENTRYPOINT). This is the native runner approach: `sudo apt-get install -y wine32 && ./run-tests-wine.sh`. Clear separation between build (Docker) and test (native runner with Wine).

**Warning signs:** Test failure reported as "Build" failure in CI summary.

### Pitfall 4: CLAUDE.md scope conflicts with personal rules

**What goes wrong:** Agent uses personal `~/.claude/rules/` which override the project CLAUDE.md, causing project-specific constraints to be silently ignored (e.g., agent adds a dependency, agent refactors BT6 paths).

**Why it happens:** Personal rules have higher precedence in some agent configurations.

**How to avoid:** Write CLAUDE.md to be explicit about what overrides what. State "These rules take precedence for this project" at the top. Document the most common violations agents make (add dependency, rename, touch GNUmakefile) as explicit prohibitions.

**Warning signs:** Agent produces code with external imports, or modifies GNUmakefile.

## Code Examples

Verified patterns from the existing codebase:

### run-tests-wine.sh (existing — aide-verify.sh wraps this)

```bash
#!/bin/bash

cd build
unzip -o tests.zip -d tests
cd tests
chmod +x run-tests.sh
./run-tests.sh
```

Source: `/home/voidderef/workspace/musicgame/djhackers/bemanitools/run-tests-wine.sh`

### dist/test/run-tests.sh (existing — executes test binaries under Wine)

```bash
#!/bin/bash
set -e

cd $DIR

echo "Running tests..."

wine ./cconfig-test.exe
wine ./cconfig-util-test.exe
# ... (all test binaries)
wine ./inject.exe d3d9hook.dll d3d9hook-test.exe

echo "All tests successful."
```

Source: `/home/voidderef/workspace/musicgame/djhackers/bemanitools/dist/test/run-tests.sh`

### TEST_MODULE_BEGIN/END pattern (for module map in agent-reference.md)

```c
TEST_MODULE_BEGIN("security-id")
TEST_MODULE_TEST(test_to_str)
TEST_MODULE_TEST(test_parse_valid)
TEST_MODULE_END()
```

Source: `src/test/test/test.h`

### Tagged output pattern for aide-verify.sh

```
[BUILD] PASS
[FORMAT] FAIL
clang-format violations found. Commit the formatted files and re-run.
[TEST] PASS
```

The tagged line must be last within each stage block so `grep '\[BUILD\]'` captures the definitive result.

### GitHub Actions step pattern (extending build-master.yaml)

```yaml
- name: "Run tests (Wine)"
  run: |
    sudo dpkg --add-architecture i386
    sudo apt-get update
    sudo apt-get install -y wine wine32
    ./run-tests-wine.sh
```

## State of the Art

| Old Approach | Current Approach | Notes |
|---|---|---|
| Manual build + test | `make release` (build + format + test in one) | `make release` exists but is destructive (runs format in place); agent needs non-destructive verify |
| No agent constraints | New CLAUDE.md | First project-level agent constraint file |
| No structured verify output | aide-verify.sh | Adds machine-parseable tagged lines |
| Build-only CI | CI with Wine tests | Extends existing pipeline |

## Open Questions

1. **Wine version on Debian 11 in Docker vs ubuntu-22.04 native**
   - What we know: STATE.md flags "Wine version on CI runners (ubuntu-22.04 ships Wine 7.x)" as a concern. Debian 11 ships Wine ~6.x. The test executables use standard Win32 APIs with no version-specific features.
   - What's unclear: Whether Wine 6.x (Debian 11) passes all tests that Wine 7.x (ubuntu-22.04) would. The tests in `dist/test/run-tests.sh` are unit tests for pure logic — they should work on Wine 6+.
   - Recommendation: Use the native runner (ubuntu-22.04) with `sudo apt-get install -y wine32` for CI tests. This gives Wine 7.x (or whatever ubuntu-22.04 packages), matches the environment where developers run tests locally on Ubuntu, and avoids touching Dockerfile.build. The locked decision says to extend Dockerfile.build — if the planner follows that, add `wine wine32` to Dockerfile.build's apt-get install and run tests inside the container via a separate `docker run` step.

2. **`--dry-run --Werror` in clang-format version on CI**
   - What we know: `clang-format --dry-run --Werror` is available in clang-format 10+. The project doesn't pin a clang-format version; developers install whatever their distro ships.
   - What's unclear: Which clang-format version is on the CI runner or in the Docker image.
   - Recommendation: Use `git diff --exit-code` approach (does not depend on clang-format version). Document this in aide-verify.sh as the canonical diff-check method.

3. **aide-verify.sh: keep run-tests-wine.sh separate or inline?**
   - What we know: CONTEXT.md marks this as "Claude's Discretion."
   - Recommendation: Keep `run-tests-wine.sh` separate. It is the existing test runner; `aide-verify.sh` invokes `make run-tests` which calls it. No consolidation needed — the indirection is fine and avoids duplicating Wine invocation logic.

## Sources

### Primary (HIGH confidence)

- Direct codebase inspection — `run-tests-wine.sh`, `dist/test/run-tests.sh`, `GNUmakefile`, `Dockerfile.build`, `.github/workflows/build-master.yaml`, `src/test/test/check.h`, `src/test/test/test.h`
- `.planning/codebase/` — STACK.md, TESTING.md, STRUCTURE.md, ARCHITECTURE.md, CONVENTIONS.md (all dated 2026-02-28, project-specific analysis)
- `.planning/phases/01-agent-foundation/01-CONTEXT.md` — User decisions (2026-03-01)

### Secondary (MEDIUM confidence)

- Debian 11 Wine packaging: Wine 6.x expected in Debian 11 (bullseye) based on historical package versions; verify with `apt-cache show wine` in container during implementation
- `clang-format --dry-run --Werror`: Available since LLVM/clang-format 10 (released 2020); likely available on ubuntu-22.04 but not confirmed for this project's toolchain

### Tertiary (LOW confidence)

- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all tools are from the existing codebase; nothing new introduced
- Architecture: HIGH — all patterns derived from existing code and locked user decisions
- Pitfalls: MEDIUM — format diff and Wine masking pitfalls are derived from code inspection; Wine version compatibility is inferred

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable domain — shell scripts, GitHub Actions, Wine)
