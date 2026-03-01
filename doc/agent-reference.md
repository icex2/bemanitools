# Agent Codebase Reference

Agent-oriented orientation guide. Read this first. Link out to existing docs for depth — do not expect this file to be exhaustive.

## Quick-Start: Build and Verify

```bash
./scripts/aide-verify.sh
```

Runs three stages in order: BUILD → FORMAT → TEST. Each prints `[STAGE] PASS` or `[STAGE] FAIL`. On FORMAT FAIL, clang-format has modified files in place — commit the formatted files and re-run.

For full environment setup (toolchain, Wine, Docker), see [doc/development.md](development.md).

Cross-compile target: **Windows (Win32/Win64) via MinGW-w64 on Linux.** Tests run under Wine.

Build quick-reference:

| Command | What it does |
|---------|-------------|
| `make build` | Compile all targets |
| `make code-format` | Apply clang-format in place |
| `make run-tests` | Run all test executables under Wine |
| `scripts/aide-verify.sh` | Full verification (build + format check + tests) |

## Module Map by Tier

Modules live under `src/main/`. Tier determines testability without hardware.

### Tier 1 — Pure Logic (unit-testable, no hardware)

Write tests here. Changes must pass `make run-tests`.

| Module | Description |
|--------|-------------|
| `cconfig/` | Config file parsing library |
| `security/` | Konami security emulation (ID, mcode, rp, rp2, rp3, util) |
| `util/` | Logging, memory, threading, network utilities |
| `iidxhook-util/` (config subset) | Config parsing helpers shared across IIDX hooks |

### Tier 2 — Emulation (partially testable via Wine, no hardware needed)

Some modules have tests; others can be tested with synthetic inputs. No physical hardware required.

| Module | Description |
|--------|-------------|
| `ezusb-emu/`, `ezusb-iidx-emu/` | ezusb emulation state machines |
| `ezusb2-emu/`, `ezusb2-iidx-emu/` | ezusb2 emulation state machines |
| `acioemu/`, `bio2emu/` | ACIO and BIO2 board emulation |
| `p3ioemu/`, `p4ioemu/` | P3IO and P4IO board emulation |
| `d3d9hook/` | D3D9 hooking (has automated test: `d3d9hook-test`) |
| `iidxhook8/` | IIDX hook for games 25-26 (has automated test: `iidxhook8-test`) |

### Tier 3 — Hooks / Hardware-Dependent (manual testing only)

Do not attempt automated testing. Changes require a real game + hardware to verify.

| Category | Modules |
|----------|---------|
| Game hook DLLs | `iidxhook1-8/`, `ddrhook1-2/`, `jbhook1-3/`, `popnhook1/`, `sdvxhook/`, `sdvxhook2/`, `bsthook/` |
| I/O dispatcher DLLs | `iidxio/`, `ddrio/`, `eamio/`, `jbio/`, `popnio/`, `sdvxio/` |
| Hardware drivers | `ezusb/`, `ezusb2/`, `bio2/`, `bio2drv/`, `acio/`, `aciodrv/` |
| Hook infrastructure | `hook/`, `hooklib/`, `inject/`, `launcher/` |

## Test Coverage Map

Tests are in `src/test/`. Each test module compiles to a standalone `.exe` run under Wine.

| Module | Test file(s) | What's covered |
|--------|-------------|----------------|
| `cconfig` | `cconfig-test`, `cconfig-util-test`, `cconfig-cmd-test` | Init, get/set, utility functions, CLI parsing |
| `security` | `security-id-test`, `security-mcode-test`, `security-util-test`, `security-rp-test`, `security-rp2-test`, `security-rp3-test` | ID parse/verify, mcode, response processing |
| `util` | `util-net-test` | Network utilities |
| `iidxhook-util` | `iidxhook-util-config-eamuse-test`, `iidxhook-util-config-gfx-test`, `iidxhook-util-config-misc-test`, `iidxhook-util-config-sec-test` | Config section parsing |
| `d3d9hook` | `d3d9hook-test` (via `inject.exe`) | D3D9 hook integration |
| `iidxhook8` | `iidxhook8-test` | IIDX hook 8 integration |

**To add a test:** create `src/test/{module}/` with a `Module.mk` and a `{module}-test.c` using the `TEST_MODULE_BEGIN/END` pattern. See [Testing Patterns](#test-framework).

## Hook Architecture Overview

See [doc/architecture.md](architecture.md) for the full picture. In brief:

```
game binary
  └─ inject.exe injects hook DLL into process
       └─ hook DLL patches IAT (Import Address Table)
            └─ intercepted Win32 calls → bemanitools handlers
                 ├─ hardware emulation (ezusb-emu, acioemu, bio2emu, ...)
                 ├─ I/O API (iidxio, ddrio, eamio, ...)
                 └─ graphics / misc patches (d3d9hook, iidxhook-util, ...)
```

Key pattern: the hook DLL is injected before game code runs. IAT patching replaces function pointers in the game's import table so all calls to hooked APIs go through bemanitools first.

## Key Conventions

C99, `-Wall` clean. 80-column limit, 4-space indent, Linux brace style — enforced by `.clang-format`. See [doc/development.md](development.md) for full style guide.

**Test framework:** Custom `check.h` macros. Do not introduce external test frameworks.

```c
TEST_MODULE_BEGIN("module-name")
TEST_MODULE_TEST(test_my_function)
TEST_MODULE_END()

static void test_my_function(void)
{
    check_int_eq(some_func(1), 42);
    check_str_eq(some_str_func("x"), "expected");
    check_bool_true(some_bool_func());
}
```

**New module checklist:**
1. Create `src/main/{module}/` with source files and `Module.mk`
2. Add `include src/main/{module}/Module.mk` to root `Module.mk` (requires approval — see CLAUDE.md)
3. If testable: create `src/test/{module}/` with its own `Module.mk`
4. Run `scripts/aide-verify.sh` before committing

**Function naming:** `{module}_{action}()` — e.g., `cconfig_init()`, `security_id_parse()`.
**Constants:** `SCREAMING_SNAKE_CASE`.
**Structs:** `struct {module}_{name}` — e.g., `struct cconfig`, `struct security_id`.
