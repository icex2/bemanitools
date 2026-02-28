# Coding Conventions

**Analysis Date:** 2026-02-28

## Language & Core

**Primary Language:** C99 (C with C99 standard library)

**Compilation:**
- Flags: `-O2 -pipe -ffunction-sections -fdata-sections -Wall -std=c99`
- Windows-only: `-DWIN32_LEAN_AND_MEAN -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DCOBJMACROS`
- Release adds: `-Werror` (all warnings become errors)
- Location: `GNUmakefile` lines 30-33

## Naming Patterns

**Files:**
- Kebab-case with module prefix: `eamio-icca.c`, `security-id-test.c`, `config-icc.h`
- Test files append `-test`: `security-id-test.c`, `cconfig-test.c`
- Module naming reflects functionality, not file path

**Functions:**
- Snake_case prefix with module name: `eam_io_init()`, `eam_io_poll()`, `cconfig_set()`
- Internal static functions prefix with underscore: `_mode_invalid()`, `_check_input_state_change()`
- Init/finalize pairs: `cconfig_init()` / `cconfig_finit()` (note: `finit` not `fini`)
- Boolean return functions don't follow `is_*` pattern; they return `bool` with function name indicating the action: `check_if_icca()`, `security_id_verify()`

**Variables:**
- Snake_case for all local and global variables
- Static module-level state prefixed with module name: `eam_io_icca_state`, `icca_node_id`, `acio_manager_ctx`
- Pointer suffix `_ctx` for context structures: `acio_manager_ctx`, `nv_api`
- Temporary variables use short names: `tmp`, `i`, `hr` (for HRESULT)
- Array length prefix `n` or `n_`: `nentries`, `nbytes`, `nsrc`

**Types:**
- Struct names use snake_case with module prefix: `struct cconfig`, `struct icc_config`, `struct mode_test_output_state`
- Typedef patterns: rarely used; prefer `struct cconfig` over typedef alias
- Enum for choices in mode dispatch: `enum mode` with values like `MODE_INVALID`, `MODE_SCAN`
- Boolean usage: `bool` type preferred over `int` for boolean returns

**Constants:**
- SCREAMING_SNAKE_CASE for all defines and const values: `IDLE_RESPONSES_BETWEEN_FELICA_POLLS`, `NUMBER_OF_EMULATED_READERS`, `INVALID_NODE_ID`
- Module-specific configs prefix with module name in uppercase: `EAMIO_ICCA_CONFIG_ICC_PORT_KEY`, `EAMIO_ICCA_CONFIG_ICC_DEFAULT_PORT_VALUE`

## Code Style

**Formatting:**
- Tool: `clang-format` with `.clang-format` config
- Column limit: 80 characters
- Indentation: 4 spaces, no tabs
- Run formatting before commit: `make code-format` or `clang-format -i -style=file`
- Location: `.clang-format` defines all formatting rules

**Key clang-format settings:**
- `AlignAfterOpenBracket: AlwaysBreak` - Break after opening parenthesis in function calls
- `BinPackArguments: false` - Each argument on separate line
- `BinPackParameters: false` - Each parameter on separate line
- `BreakBeforeBraces: Linux` - Linux-style braces
- `PointerAlignment: Right` - Pointer symbol attached to type: `const char *var`
- `KeepEmptyLinesAtTheStartOfBlocks: true` - Preserve empty lines at block start
- `SpaceAfterCStyleCast: true` - Space after cast: `(int) x`

**Linting:**
- Tool: GCC with `-Wall` enabled
- Release builds use `-Werror`: all warnings must be fixed
- No compiler warnings in committed code

## Import Organization

**Order:**
1. System headers (C standard library): `<stdio.h>`, `<stdint.h>`, `<windows.h>`
2. Windows-specific includes: `<dbt.h>`
3. Internal project headers: quoted paths like `"aciodrv/device.h"`
4. Module-specific headers: `"bemanitools/eamio.h"`
5. Config headers: `"cconfig/cconfig-main.h"`
6. Utilities: `"util/log.h"`

**Notes:**
- `#include <windows.h>` sometimes requires directive comments: `// clang-format off` before and `// clang-format on` after if order matters (`windows.h` must come before `dbt.h`)
- Location: `src/main/eamio-icca/eamio-icca.c` lines 1-20 show proper order

**Path Aliases:**
- No path aliases in use; all includes are relative to `src/main` or `src/test` set in compiler via `-I` flags
- Build system defines `-I src -I src/main -I src/test`

## Error Handling

**Pattern - Function Returns:**
- Functions indicate failure by returning `false` (for bool return): `return false;`
- Pointer-returning functions indicate failure with `NULL`: `return NULL;`
- No exceptions; errors are checked at call site
- Early return on error is standard: check condition, log warning, return false

**Pattern - Logging on Error:**
```c
// Example from src/main/eamio-icca/eamio-icca.c:114-115
if (!cconfig_util_get_str(...)) {
    log_warning("Invalid value for key '%s' specified, fallback to default '%s'", ...);
}
```

**Assertion Usage:**
- `assert()` used in some modules for invariants, not typical
- Safer: use explicit checks with error return

**No Exception-like Mechanisms:**
- setjmp/longjmp: not used
- Signals: not used
- All errors flow through return values

## Logging

**Framework:** Custom logging system accessed via `util/log.h`

**Module Declaration:**
- File-level macro at top of implementation file: `#define LOG_MODULE "module-name"`
- Location: before first include or after includes
- Examples: `#define LOG_MODULE "p3io-ddr-tool"` in `src/main/p3io-ddr-tool/main.c:1`

**Logging Levels:**
- `log_misc()` - Miscellaneous details
- `log_info()` - Informational messages
- `log_warning()` - Warning messages
- `log_set_level()` - Set verbosity threshold
- `log_to_writer()` - Direct stderr output: `log_to_writer(log_writer_stderr, NULL);`
- `log_to_external()` - Send logs to external formatter

**Patterns:**
- Call `log_to_writer(log_writer_stderr, NULL)` in test/main setup to enable logging
- Use `log_warning()` for initialization failures or config issues
- Use `log_info()` for normal operation milestones
- Log on failure before returning error: failure message should appear in output

## Comments

**When to Comment:**
- Explain WHY, not WHAT: "all of these are referred to internally as ICCA" is good
- Clarify non-obvious logic, especially bit operations or hardware interaction
- Flag workarounds: `// this node is not setup, just return "success"`
- Document data ordering requirements: `// Don't format because the order is important here`

**When NOT to Comment:**
- Don't comment obvious variable names or straightforward loops
- Don't repeat the code in English: `i++; // increment i` is noise

**Format:**
- Single-line comments use `//` (C99 style)
- Block comments use `/* */` when multiple lines
- No javadoc-style comments; C89-style preferred for clarity
- Keep comments close to code they describe

## Function Design

**Size Guidelines:**
- Aim for functions under 50 lines; functions over 100 lines should be reviewed for refactoring
- Common pattern: init/finalize pairs split into separate functions
- Mode dispatch uses typedef'd function pointers: `typedef bool (*mode_proc)(HANDLE handle);`

**Parameters:**
- Pass structures by pointer, not by value: `struct cconfig *config`
- Output parameters passed as pointer: `void security_id_parse(const char *str, struct security_id *id)`
- Avoid excessive parameters; 4-5 is typical max
- Use pointers for optional output: `NULL` means don't store result

**Return Values:**
- `bool` for success/failure operations
- `NULL` for pointer-returning functions on failure
- Integer return for error codes (rare; prefer bool)
- No output-via-parameters pattern (prefer return struct where possible)

## Module Design

**File Organization:**
- Module implementation: `src/main/module-name/module-name.c`
- Module header: `src/main/module-name/module-name.h` (if public API)
- Sub-components: `src/main/module-name/subname.c` and `subname.h`
- Tests for module: `src/test/module-name-test/module-name-test.c`

**Header Guards:**
- All headers have include guards: `#ifndef MODULE_NAME_H #define MODULE_NAME_H ... #endif`
- Format: Uppercase module name with underscores, all in caps: `#ifndef SECURITY_ID_H`

**Exports:**
- Only public functions declared in header
- Internal helper functions use `static` keyword
- No global variables exposed in headers (module-level state is always static)
- State managed via returned pointers: `struct cconfig *config = cconfig_init();`

**No Barrel Files:**
- Each module has its own header; no aggregate include files
- Direct paths required: `#include "security/id.h"` not `#include "security.h"`

**Building Modules:**
- Each module declares itself in `Module.mk`
- Each test declares itself in its respective test directory's `Module.mk`
- Location: `src/test/security/Module.mk` shows test module structure
- Build system uses this to determine which source files compile for each module

## Platform-Specific Code

**Windows-Only:**
- Target: 32-bit and 64-bit Windows via MinGW cross-compilation
- Headers like `<windows.h>`, `<dbt.h>` wrapped when needed
- HRESULT type and error codes used for Windows APIs

**Conditional Compilation:**
- Not used heavily; code is usually Windows-only without ifdef
- When needed: preprocessor guards but rare in this codebase

---

*Convention analysis: 2026-02-28*
