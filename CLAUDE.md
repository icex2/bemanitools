# CLAUDE.md — Bemanitools Agent Constraints

Bemanitools 5 (BT5): Windows DLL injection toolkit for Konami arcade rhythm games. Cross-compiled from Linux via MinGW-w64. All code targets Win32/Win64 — no Linux or Mac runtime targets.

For codebase orientation, module tiers, and test coverage: see [doc/agent-reference.md](doc/agent-reference.md).

## Scope Constraints

- **BT5 only.** Do not touch BT6 refactoring branches or create BT6 code.
- **No new game series.** Extend only games already supported.
- **No new dependencies** without explicit approval from the user. The project is self-contained by design.
- **Windows-only target.** All code compiles for Win32 via MinGW cross-compilation. Do not add Linux-native or POSIX-only code paths.

## Verification Workflow

Run before every commit:

```bash
./scripts/aide-verify.sh
```

All three stages must PASS: `[BUILD] PASS`, `[FORMAT] PASS`, `[TEST] PASS`.

If FORMAT FAIL: clang-format has modified files in place. Inspect `git diff`, commit the formatted files, then re-run. Do not skip this step.

## Coding Conventions

- **C99 standard, `-Wall` clean.** No warnings in committed code.
- **Formatting:** `clang-format` with project `.clang-format` (80-col, 4-space indent, Linux braces). Run `make code-format` before committing.
- **Naming:** snake_case functions prefixed with module name (`cconfig_init`, `security_id_parse`), SCREAMING_SNAKE_CASE constants, `struct {module}_{name}` structs.
- **Test framework:** Custom `check.h` macros (`TEST_MODULE_BEGIN/END`, `check_int_eq`, `check_str_eq`, etc.). Do not introduce external test frameworks (gtest, CMocka, Unity, etc.).
- **New module:** create `src/main/{module}/` with `Module.mk` — follow existing examples (e.g., `src/main/cconfig/Module.mk`).

## Do-Not-Touch List

Do not modify these without explicit approval:

- `GNUmakefile` — root build orchestration; complex and fragile
- Root `Module.mk` — module inclusion list
- `.clang-format` — established formatting rules
- `imports/` — third-party import definitions

## Build Quick-Reference

| Command | Purpose |
|---------|---------|
| `make build` | Compile all targets |
| `make code-format` | Apply clang-format in place |
| `make run-tests` | Run tests under Wine |
| `scripts/aide-verify.sh` | Full verification loop |
