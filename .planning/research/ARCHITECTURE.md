# Architecture Research

**Domain:** Agentic development infrastructure for a DLL injection / reverse engineering codebase
**Researched:** 2026-03-01
**Confidence:** HIGH (based on direct codebase analysis)

## Standard Architecture

The AIDE infrastructure wraps the existing Bemanitools layered architecture. There are two distinct runtime worlds:

- **Isolated** — code that runs standalone as Windows .exe/.dll cross-compiled with MinGW, testable under Wine on Linux without any game binary.
- **Injected** — code that only executes inside a live game process (bm2dx.exe, ddr.exe, etc.), where testing requires the actual copyrighted Konami binary.

Everything AIDE builds must account for this split. The vast majority of correctness bugs live in isolated layers (protocol parsers, config, security math, message node state machines), not in the game-process hooks themselves.

### System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                      AIDE Infrastructure                            │
│                                                                     │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  │
│  │  Reference DB    │  │  Build Pipeline  │  │  Test Harness    │  │
│  │  (.planning/     │  │  (GNUmakefile +  │  │  (src/test/ +   │  │
│  │   reference/)    │  │   Docker + Wine) │  │   Wine runner)  │  │
│  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘  │
│           │                     │                     │             │
│           └─────────────────────┼─────────────────────┘             │
│                                 │                                   │
│                    ┌────────────▼────────────┐                      │
│                    │   Verification Layer    │                      │
│                    │  (build + test + log    │                      │
│                    │   analysis workflow)    │                      │
│                    └────────────┬────────────┘                      │
└─────────────────────────────────┼───────────────────────────────────┘
                                  │
┌─────────────────────────────────▼───────────────────────────────────┐
│                      Bemanitools Codebase                           │
│                                                                     │
│  ISOLATED (testable under Wine without game binary)                 │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌──────────────┐  │
│  │  util/     │  │  security/ │  │  cconfig/  │  │  hardware    │  │
│  │  log,thread│  │  id,mcode, │  │  config    │  │  protocol    │  │
│  │  crypto    │  │  rp,rp2,3  │  │  parsers   │  │  parsers     │  │
│  └────────────┘  └────────────┘  └────────────┘  └──────────────┘  │
│                                                                     │
│  PARTIALLY ISOLATED (stateful emulators, no game binary needed)     │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌──────────────┐  │
│  │ ezusb-emu/ │  │ bio2emu/   │  │ acioemu/   │  │ p3ioemu/     │  │
│  │ message    │  │ board      │  │ serial     │  │ p4ioemu/     │  │
│  │ state mach │  │ emulation  │  │ emulation  │  │ emulation    │  │
│  └────────────┘  └────────────┘  └────────────┘  └──────────────┘  │
│                                                                     │
│  INJECTED (requires live game process — no automated testing)       │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌──────────────┐  │
│  │ iidxhook*/ │  │ ddrhook*/  │  │ jbhook*/   │  │ popnhook*/   │  │
│  │ dllmain.c  │  │ dllmain.c  │  │ dllmain.c  │  │ dllmain.c    │  │
│  │ d3d9 hooks │  │ d3d9 hooks │  │ d3d9 hooks │  │ d3d9 hooks   │  │
│  └────────────┘  └────────────┘  └────────────┘  └────────────── ┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Location |
|-----------|----------------|----------|
| Reference DB | Store protocol specs, decompiled headers, API contracts, game-specific notes for agent context | `.planning/reference/` (new) |
| Build Pipeline | Cross-compile Windows binaries via MinGW on Linux; Docker for reproducibility; Wine for test execution | `GNUmakefile`, `Dockerfile.build`, `run-tests-wine.sh` |
| Test Harness | Unit tests for isolated modules; custom C framework (`TEST_MODULE_BEGIN/END`, `check_*` macros) | `src/test/`, `dist/test/run-tests.sh` |
| Verification Layer | Workflow script or protocol: build → run Wine tests → interpret results → feed back to agent | New, in `scripts/` or `.planning/` |
| Hook Architecture | Inject.exe → launcher → hook DLLs → game binary (existing, unchanged) | `src/main/inject/`, `src/main/launcher/`, `src/main/iidxhook*/` |

## Component Boundaries: What Can Be Tested in Isolation

This is the most critical architectural question for AIDE. The boundary is defined by whether execution requires a live game process.

### Isolated (testable today via Wine, no game needed)

These modules compile to standalone .exe or can be exercised without a running game process:

- `util/` — logging, threading, memory, crypto, net utilities
- `security/` — id, mcode, rp, rp2, rp3 algorithms (pure math/crypto)
- `cconfig/` — config file parsing (pure file I/O)
- `iidxhook-util/config-*.c` — config struct parsing (pure C, no Win32)
- Message node state machines in `ezusb-iidx-emu/`, `bio2emu/`, `acioemu/` — stateful but self-contained
- Protocol parsers in `ezusb-iidx/`, `bio2/`, `acio/` — message serialization/deserialization

**These are the right targets for AIDE test expansion.** All 21 existing test files cover exactly this tier.

### Partially Isolated (can be stub-tested with effort)

These modules require Windows APIs but not game binaries. They could be tested with stubs or under Wine with mocked devices:

- `hooklib/rs232.c` — serial port; mockable with Wine's COM port emulation
- `hooklib/setupapi.c` — device enumeration; needs SetupAPI (available in Wine)
- `hooklib/memfile.c` — memory-mapped files; testable under Wine
- `geninput/` — keyboard/mouse input; partially testable under Wine
- `cconfig/` + `config/` — config UI (Wine + windowed mode)

### Injected (requires game binary — no automated testing feasible)

These modules only execute inside a live game process:

- All `*hook*/dllmain.c` — initialization runs at DLL injection time only
- `hook/d3d9.c`, `hook/iohook.c`, `hook/table.c` — hooks fire against game's import table
- `d3d9exhook/`, `d3d9-util/`, `d3d9-frame-graph-hook/` — require a running D3D9 device
- AVS-version-specific code paths in hook DLLs — require real AVS .dll present

**Verification here is manual or log-based.** AIDE cannot run these automatically. Instead, AIDE should assist by analyzing logs from manual test runs and cross-referencing against reference material.

## Recommended AIDE Project Structure

```
.planning/
├── reference/                  # Reference DB: feeds agent context
│   ├── protocols/              # Hardware protocol docs (ezusb, BIO2, ACIO, P3IO, P4IO)
│   │   ├── ezusb-iidx.md       # IIDX ezusb message format reference
│   │   ├── bio2.md             # BIO2 serial protocol reference
│   │   ├── acio.md             # ACIO serial protocol reference
│   │   └── p3io-ddr.md         # P3IO DDR cabinet protocol reference
│   ├── apis/                   # Bemanitools API contracts
│   │   ├── iidxio.md           # iidxio.h annotated reference
│   │   ├── ddrio.md            # ddrio.h annotated reference
│   │   ├── eamio.md            # eamio.h annotated reference
│   │   └── avs.md              # AVS framework hooks and versioning
│   ├── games/                  # Game-specific notes per version
│   │   ├── iidx/               # Per IIDX version: boot sequence, known hooks, regression notes
│   │   ├── ddr/                # DDR version notes
│   │   └── popn/               # pop'n music version notes
│   └── bugs/                   # Bug-specific research artifacts
│       ├── popn-regression-5.44.md   # Root cause hypothesis, relevant code paths
│       └── iidx-timing-344.md        # Issue analysis for #344
├── research/                   # Research files (this directory)
├── PROJECT.md
├── REQUIREMENTS.md
└── config.json

scripts/
└── aide-verify.sh              # Build + Wine test wrapper for agentic iteration
```

### Structure Rationale

- `reference/protocols/` feeds agents working on hardware emulator bugs — protocol specs give ground truth for message parsing
- `reference/apis/` is the contract layer — when an agent modifies an I/O implementation, it can verify against the declared API
- `reference/games/` captures per-version boot sequences so agents know what a successful run looks like (what to expect in logs)
- `reference/bugs/` stores per-issue analysis so agents don't re-derive context on every session
- `scripts/aide-verify.sh` provides a single agentic entry point: build → test → report result

## Architectural Patterns

### Pattern 1: Isolated Unit Test Expansion

**What:** Add test files under `src/test/{module}/` following the existing `TEST_MODULE_BEGIN/END` + `check_*` macro framework. Wire new tests into `dist/test/run-tests.sh`.

**When to use:** For any pure logic module that does not call Windows APIs requiring a device or D3D device. Protocol parsers, state machine transitions, config struct validation, security algorithms.

**Trade-offs:** High value, zero setup cost. The Wine-based runner already works. No mocking infrastructure required — just call the C functions directly.

```c
// Pattern: src/test/bio2emu/bio2emu-state-test.c
#include "bio2emu/bio2emu.h"
#include "test/check.h"
#include "test/test.h"

static void test_init_state() {
    struct bio2emu_port port;
    bio2emu_port_init(&port);
    check_bool_true(bio2emu_port_is_ready(&port));
}

TEST_MODULE_BEGIN("bio2emu-state")
TEST_MODULE_TEST(test_init_state)
TEST_MODULE_END()
```

### Pattern 2: Log-Based Verification for Injected Code

**What:** For hook DLL behavior that cannot be unit tested, structure log output so it's machine-parseable. Define expected log patterns per game version and boot stage in `reference/games/{game}/`. AIDE agents verify a manual run by diffing actual log against expected patterns.

**When to use:** Regression verification after any change to a hook DLL's `dllmain.c` or initialization sequence. This is the only automated verification possible for injected code.

**Trade-offs:** Requires manual test runs — AIDE cannot drive the game. But it can analyze the resulting log, which is better than nothing. The existing `log.c` infrastructure already writes to file.

```
# reference/games/popn/boot-expected.txt
[INFO] popnhook1: DLL_PROCESS_ATTACH
[INFO] cconfig: Loaded config from popnhook-15.conf
[INFO] popnhook1: ezusb2-popn-emu initialized
[INFO] popnhook1: Boot complete
```

### Pattern 3: Agentic Verification Script

**What:** A single script (`scripts/aide-verify.sh`) that builds a specific module or all modules, runs Wine tests, and exits 0/non-0 to signal success/failure. Structured output (prefixed lines) makes it parseable by an agent.

**When to use:** After every code change. The agent runs this, reads stdout, and decides whether to iterate or commit.

**Trade-offs:** MinGW cross-compilation is fast (single modules rebuild in seconds). Wine test execution is slower (~10-30s for full suite). The agent should build targeted modules first, then run full suite before marking done.

```bash
#!/bin/bash
# scripts/aide-verify.sh [module]
# Exit 0 = success, non-0 = failure
# Structured output: [BUILD] [TEST] [ERROR] prefixes

MODULE=${1:-all}
echo "[BUILD] Building ${MODULE}..."
make ${MODULE} 2>&1 | sed 's/^/[BUILD] /'
if [ $? -ne 0 ]; then echo "[ERROR] Build failed"; exit 1; fi

echo "[TEST] Running Wine tests..."
./run-tests-wine.sh 2>&1 | sed 's/^/[TEST] /'
```

### Pattern 4: Reference Material as Agent Context

**What:** Before working on any module, the agent reads the relevant reference file from `.planning/reference/`. For protocol bugs, this means reading the protocol spec first, then cross-referencing against the implementation. Reference files are markdown — compact enough to fit in context, structured enough to be queryable.

**When to use:** Any time an agent is asked to fix a bug in hardware emulation or game hook code. Without reference material, the agent is working from C source alone, which often lacks comments explaining what the hardware actually does.

**Trade-offs:** Reference material must be maintained — it goes stale if the protocol understanding evolves. Keep reference files scoped to known-stable facts (wire protocol formats, API contracts) rather than speculative implementation notes.

## Data Flow: How Reference Material Reaches Agent Context

```
Bug report / issue
        |
        v
.planning/reference/bugs/{issue}.md   <-- agent reads this first
        |
        v
.planning/reference/protocols/{hw}.md  <-- hardware protocol ground truth
        |
        v
src/main/{module}/                     <-- implementation to verify against reference
        |
        v
src/test/{module}/                     <-- existing + new unit tests
        |
        v
scripts/aide-verify.sh                 <-- build + test execution
        |
        v
log output / test results              <-- agent reads, compares to reference/games/
        |
        v
fix or iterate
```

The critical insight is that reference material is a pre-processing step, not an afterthought. An agent without protocol docs is guessing at the purpose of magic numbers in `ezusb-iidx-emu/`. An agent with the protocol doc can immediately verify whether a message handler is correct.

## Build Order for Maximum Leverage

Build AIDE infrastructure in this order, each step enabling the next:

**Step 1: Build script wrapper (1-2 hours)**
Create `scripts/aide-verify.sh` that wraps `make` + `run-tests-wine.sh`. This immediately gives agents a single command to verify any change. The existing build system is already correct — this is just a wrapper.

**Step 2: Document build workflow in reference (1 hour)**
Write `.planning/reference/build-workflow.md` explaining how to build a single module (`make build/bin/indep-32/security-id-test.exe`), what Wine requires, and how to interpret test output. This is the AIDE-04 requirement.

**Step 3: Protocol reference docs (highest value, most effort)**
Populate `.planning/reference/protocols/` starting with ezusb-iidx (IIDX is the largest game family) and bio2. These feed agents working on hardware emulation bugs directly. Pull from `doc/` where it exists, supplement by reading source.

**Step 4: Game boot sequence docs (enables log verification)**
Write `.planning/reference/games/{game}/boot-expected.txt` for the games with active regressions (popn first, IIDX second). This enables structured log comparison as a regression test proxy.

**Step 5: Expand unit tests for emulator state machines**
Extend `src/test/` to cover `ezusb-iidx-emu/`, `bio2emu/`, `acioemu/` message node state machines. These are isolated modules with no game binary dependency — currently untested despite being where protocol bugs live.

**Step 6: Bug-specific reference artifacts**
For each active bug, write a `.planning/reference/bugs/{issue}.md` with: symptom, affected code paths, relevant protocol sections, and reproduction steps. This replaces re-deriving context from scratch on each session.

## Integration Points

### Hook Architecture Integration

| Boundary | AIDE Touchpoint | Notes |
|----------|----------------|-------|
| inject.exe → game process | Log analysis only | Cannot intercept automatically |
| hook DLL initialization | Reference docs (boot sequence) | Expected log patterns per game |
| Bemanitools API contracts | `reference/apis/` | Spec for verifying implementation correctness |
| Hardware emulator state | Unit tests (new) | Most feasible test expansion target |
| Config system | Unit tests (existing) | Well covered already |
| Security algorithms | Unit tests (existing) | Full coverage in src/test/security/ |

### Build Pipeline Integration

The existing build pipeline (`make` + Docker + Wine) requires no changes for agentic use. Key facts for agents:

- `make build/bin/indep-32/{module}.exe` builds a single module — fast iteration
- `make` (full build) takes ~minutes inside Docker; avoid for single-module iteration
- Wine tests run on Linux without Windows VM; the runner is `run-tests-wine.sh`
- AVS-version builds use `-DAVS_VERSION=X`; agents must specify the right variant when building hook DLLs

## Anti-Patterns

### Anti-Pattern 1: Treating Hook DLL Bugs as Testable

**What people do:** Try to write unit tests for `iidxhook1/dllmain.c` behavior or D3D9 hook behavior.

**Why it's wrong:** These functions execute inside a live game process, hooking into the game's import table. There is no way to call them in isolation — `DLL_PROCESS_ATTACH` code depends on the game's memory layout, loaded DLLs, and live D3D9 context.

**Do this instead:** Extract the logic being tested into a pure helper function in `iidxhook-util/` or `cconfig/` that takes inputs and returns outputs without side effects. Test that helper function. The dllmain.c glue remains untested; that's acceptable.

### Anti-Pattern 2: Storing Protocol Reference in Source Comments

**What people do:** Add protocol knowledge as inline code comments (e.g., `/* FPGA command 0x72 = coin counter reset */`).

**Why it's wrong:** Comments drift, get deleted in refactors, and cannot be queried efficiently by an agent. Protocol knowledge is also cross-cutting — the same ezusb command appears in multiple source files.

**Do this instead:** Store protocol specs in `.planning/reference/protocols/{hw}.md` with stable, versioned documentation. Source code comments can reference the spec by section, but the ground truth lives in the reference file.

### Anti-Pattern 3: Running Full Docker Build for Single-Module Iteration

**What people do:** Run `make build-docker` to verify a small change.

**Why it's wrong:** Docker build rebuilds the entire MinGW toolchain container — takes 5-10 minutes. For an agent doing rapid fix-verify cycles, this is a blocking loop killer.

**Do this instead:** Install MinGW locally in the dev environment (or use `Dockerfile.dev` as a persistent interactive container). Agents use `make build/bin/indep-32/{module}.exe` directly. Docker full-build is for CI only.

### Anti-Pattern 4: Writing Reference Docs Without Cross-Referencing Source

**What people do:** Write protocol docs from memory or external sources without checking against the actual implementation.

**Why it's wrong:** BT5's hardware emulation is reverse-engineered — the source IS the protocol reference in many cases. A reference doc that contradicts the source creates false confidence.

**Do this instead:** Write reference docs by reading the source first (especially `ezusb-iidx/`, `bio2/`, `acio/`), then supplementing with any external protocol documentation where available. Mark sections as "inferred from source" vs "confirmed from hardware documentation."

## Scaling Considerations

This is single-codebase, single-developer tooling. Scale is not a concern. The relevant concern is context window efficiency:

| Context Budget | Architecture Adjustment |
|----------------|------------------------|
| Small (focused bug fix) | Load only `reference/bugs/{issue}.md` + relevant protocol section |
| Medium (module refactor) | Load `reference/protocols/{hw}.md` + `reference/apis/{game}io.md` + relevant source |
| Large (regression investigation) | Full reference load + build output analysis + log comparison |

The reference DB structure is designed to support selective loading — each file is scoped to a single protocol, API, or bug rather than a monolithic document.

## Sources

- Direct codebase analysis: `src/test/`, `GNUmakefile`, `run-tests-wine.sh`, `dist/test/run-tests.sh`
- Existing architecture docs: `.planning/codebase/ARCHITECTURE.md`
- Test framework source: `src/test/test/test.h`, `src/test/test/check.h`
- Requirements: `.planning/REQUIREMENTS.md` (AIDE-01 through AIDE-04)
- All confidence levels: HIGH (all claims derived from direct source analysis, no WebSearch required)

---
*Architecture research for: Agentic development infrastructure on DLL injection codebase*
*Researched: 2026-03-01*
