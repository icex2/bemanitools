# Phase 1: Agent Foundation - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Agents can build, test, and verify changes without constant human tooling intervention. This phase delivers: a verification script with structured output, CI with test execution, a codebase reference file for agent orientation, and project-level CLAUDE.md constraints. Requirements: AIDE-01, AIDE-02, AIDE-04.

</domain>

<decisions>
## Implementation Decisions

### Verification script (aide-verify.sh)
- Three stages in fixed order: build, code-format check, tests — always all three, fail fast on first failure
- Tagged line output: `[BUILD] PASS`, `[FORMAT] FAIL`, `[TEST] PASS` — grep-friendly, matches roadmap success criteria
- Inline error output before each stage's tag so agents can diagnose without re-running
- No partial-run flags — simplicity over flexibility
- Lives in `scripts/aide-verify.sh`
- Uses existing make targets: `make build`, `make code-format` (diff check), `make run-tests`

### Codebase reference file (doc/agent-reference.md)
- Lives in `doc/` alongside existing `architecture.md` and `development.md`
- Module map grouped by tier: pure logic (testable), emulation (partially testable), hooks (hardware-dependent)
- Test coverage map: which modules have tests, which don't
- Hook architecture overview: DLL injection, IAT patching, emulator tiers
- Build/verify quick-start pointing to `aide-verify.sh` and `doc/development.md`
- Links out to existing docs for depth — not self-contained, avoids duplication

### CLAUDE.md (project-level agent constraints)
- Lives at repo root as `CLAUDE.md`
- Strict guardrails — explicitly state conventions, no room for agents to invent patterns
- Self-contained: no dependency on personal `~/.claude/rules/`, works for any contributor
- Scope constraints:
  - BT5 only: no BT6 refactoring, no new game series
  - No new dependencies without explicit approval
  - Windows-only target: all code targets Win32 via MinGW cross-compilation
  - Test-before-commit: agents must run aide-verify.sh before committing
- Coding patterns: C99, check.h macros for tests, Module.mk for new code, clang-format before commit
- Do-not-touch list: GNUmakefile, root Module.mk, .clang-format, imports/ — prevents agents from breaking the build system
- Points to `doc/agent-reference.md` for codebase orientation

### CI pipeline
- Extend existing Docker build image (Dockerfile.build) with Wine for test execution
- Keep existing trigger: push to master only
- Add Wine test execution step after build step
- Test failures block the build (red pipeline) — matches roadmap: "fails the build on test regressions"
- Keep Docker-based build approach (`make build-docker`)

### Claude's Discretion
- CI workflow structure: extend existing file vs new file
- Exact structure and section ordering within agent-reference.md
- CLAUDE.md tone and internal organization
- aide-verify.sh implementation details (error capture, color output, exit codes)
- Whether to consolidate run-tests-wine.sh into aide-verify.sh or keep it separate

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `run-tests-wine.sh`: Existing Wine test runner that unzips build/tests.zip and runs tests. aide-verify.sh should wrap or invoke this.
- `GNUmakefile` targets: `make build`, `make run-tests`, `make code-format` already exist and work.
- `.clang-format`: Configured with project conventions (80 col, 4 spaces, Linux braces).
- `doc/architecture.md`, `doc/development.md`: Existing documentation to link from agent-reference.md.

### Established Patterns
- Build system: GNUmakefile + Module.mk per module. New scripts invoke make targets, don't bypass them.
- Test framework: Custom check.h macros (check_int_eq, check_str_eq, etc.), TEST_MODULE_BEGIN/END. No external framework.
- Cross-compilation: MinGW-w64 on Linux targeting Windows. Tests run under Wine.
- CI: GitHub Actions on ubuntu-22.04 with Docker-based builds (`make build-docker`).

### Integration Points
- aide-verify.sh wraps existing make targets and run-tests-wine.sh
- CI extends `.github/workflows/build-master.yaml` and `Dockerfile.build`
- CLAUDE.md references doc/agent-reference.md
- agent-reference.md links to doc/development.md and doc/architecture.md

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 01-agent-foundation*
*Context gathered: 2026-03-01*
