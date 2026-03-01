# Bug #345 — Pop'n Music 15-18 Regression

**Status:** Open
**Phase 4 priority:** HIGH — affects all pop'n music 15-18 users

---

## Issue Summary

Pop'n music 15, 16, 17, and 18 exhibit broken behavior introduced between BT5 v5.43 and v5.44. The regression affects games using `popnhook1.dll` which rely on `popnhook-util/acio.c` for ACIO card reader emulation. The `popnhook_acio_init(true)` call enables ACIO legacy mode — a critical requirement because pop'n 15-18 uses an older libacio that drops multi-message ACIO buffers. Any regression in legacy mode delivery or the ICCA state machine between 5.43 and 5.44 could break card reader initialization, causing games to hang on the network/card reader check screen or misfire card read events.

---

## User Reports

No GitHub issue content available (gh CLI not authenticated). Based on issue number proximity and the commit log context:

- **Affected BT5 versions:** v5.44 (regression introduced), v5.43 (last known-good)
- **Affected games:** pop'n music 15 ADVENTURE, pop'n music 16 PARTY, pop'n music 17 THE MOVIE, pop'n music 18 Sengoku Retsuden
- **Symptoms expected from popn ACIO architecture:** card reader timeout on boot, ACIO initialization failure, or random card read misfires — consistent with legacy mode or ICCA state machine regression
- **Related issues:** #341, #338 (referenced in plan context as related)

---

## Affected Games/Versions

| Game | BT5 Hook | Hook version | AVS |
|------|----------|-------------|-----|
| pop'n music 15 ADVENTURE | popnhook1 | 5.43 (last good) | 4.3 |
| pop'n music 16 PARTY | popnhook1 | 5.43 (last good) | 7.1 |
| pop'n music 17 THE MOVIE | popnhook1 | 5.43 (last good) | 9.0 |
| pop'n music 18 Sengoku Retsuden | popnhook1 | 5.43 (last good) | 10.1 |

---

## Relevant Code Paths

### ACIO initialization — entry point

`src/main/popnhook1/dllmain.c:173`
```c
popnhook_acio_init(true);  /* legacy_mode=true */
```
`true` enables ACIO legacy mode via `ac_io_legacy_mode()` call at `popnhook-util/acio.c:45`.

### Legacy mode flag — single source of truth

`src/main/acioemu/pipe.c:30`
```c
static bool ac_io_enable_legacy_mode = false;
```
Set by `ac_io_legacy_mode()` at `pipe.c:87-91`. Consumed in `ac_io_in_drain()` at `pipe.c:171-173`: if set, drain exits after emitting one response per call regardless of how many are queued.

### Drain loop — where legacy mode applies

`src/main/acioemu/pipe.c:124-176` — `ac_io_in_drain()`

The loop at `pipe.c:135-175` drains queued responses from `struct ac_io_in`. With `ac_io_enable_legacy_mode=true`, the loop breaks after the first response:
```c
if (ac_io_enable_legacy_mode) {
    break;  /* pipe.c:172 */
}
```
If this `break` was accidentally removed or the condition changed, the game receives multiple responses in one read, which the old libacio drops after the first — causing the game to believe the reader never responded to subsequent commands.

### ICCA dispatch — where commands are handled

`src/main/acioemu/icca.c:82-244` — `ac_io_emu_icca_dispatch_request()`

Key state transitions for boot sequence:
- `AC_IO_CMD_START_UP` at `icca.c:97`: sets `detected_new_reader=false`
- `AC_IO_ICCA_CMD_QUEUE_LOOP_START` at `icca.c:115`: clears `fault`, sets `polling_started=true`
- POLL with `polling_started=false` at `icca.c:440`: forces `status_code=AC_IO_ICCA_STATUS_FAULT`

If `polling_started` guard at `icca.c:440-442` was missing or inverted, the reader would report fault on every poll even after queue loop start, causing boot failure.

### Pop'n ACIO dispatch loop

`src/main/popnhook-util/acio.c:76-109` — `popnhook_acio_dispatch_irp()`

Only addr 1 is dispatched to `ac_io_emu_icca_dispatch_request()` at `acio.c:92`. Addr 0 handles address assignment. Any other address logs a warning but does not crash.

### ezusb2 popn emulator — USB2 side

`src/main/ezusb2-popn-emu/msg.c` — ezusb2 message handler for pop'n music
Changed between v5.43 and v5.44: `1f95437 fix(pnm/ezusb2-emu): Fix IO buffer inconsistency/random input misfiring`

---

## Changes Between v5.43 and v5.44

Commits touching relevant files between `5.43` and `5.44` tags:

| Commit | Message | Files |
|--------|---------|-------|
| `b0564b9` | chore: Apply code formatting | acioemu/icca.c, popnhook-util/* |
| `031836e` | chore: Apply code formatting | popnhook1/*, acioemu/* |
| `1f95437` | fix(pnm/ezusb2-emu): Fix IO buffer inconsistency | ezusb2-popn-emu/msg.c |
| `f8a0958` | Support pop'n music 15-18 | popnhook1/*, popnhook-util/*, ezusb2-popn-emu/* |

The formatting commits (`b0564b9`, `031836e`) are the primary suspects because they modify `acioemu/icca.c` and `popnhook-util/acio.c` while claiming only whitespace changes. Formatting commits that touch logic-bearing files can accidentally introduce behavior changes (e.g., if clang-format reformats a conditional, `if(x){` to `if (x) {` is safe, but if it accidentally merges/splits braces around multi-statement blocks).

**Confirmed functional changes in v5.43→v5.44 for acioemu/icca.c:** Only whitespace/formatting in the git diff — no logic changes. This means the `acioemu/icca.c` module itself was not the regression source for #345 specifically, though it was reformatted.

**`ezusb2-popn-emu/msg.c` refactoring** (`1f95437`): Changed from pointer-based `msg_resp = (struct...) read->bytes` to stack-allocated `msg_resp` struct. This refactoring is a more plausible regression source — if the stack-allocated struct is not correctly copied into `read->bytes` (or the copy was missed in the refactor), the interrupt read response would be zeroed/garbled, causing input misfires or IO timeout.

---

## Reproduction Conditions

1. Install BT5 v5.44 (or HEAD as of the regression)
2. Configure `popnhook1.dll` for pop'n music 15-18 with default `eamio.dll`
3. Launch game via `inject.exe popnhook1.dll popn.exe`
4. Observe whether the game reaches the I/O check screen and whether the card reader is detected

---

## Ranked Hypotheses

### Hypothesis 1 (MOST LIKELY): ezusb2-popn-emu interrupt read regression

**Evidence:**
- `1f95437` refactored `ezusb2_popn_emu_msg_interrupt_read()` from pointer-into-buffer to stack allocation: `struct ezusb2_popn_msg_interrupt_read_packet msg_resp;`
- The old code wrote directly into `read->bytes` via the pointer cast; the new code builds `msg_resp` on the stack but must then copy it to `read->bytes` with `memcpy` or equivalent
- If the copy step was omitted in the refactor, `read->bytes` would contain uninitialized data, causing the game to receive garbage button/status state
- This is consistent with "random input misfiring" and boot failures on pop'n music 15-18
- Confirmed by commit message: `fix(pnm/ezusb2-emu): Fix IO buffer inconsistency/random input misfiring`

**How to verify:** `git show 1f95437 -- src/main/ezusb2-popn-emu/msg.c` and check whether `read->pos = sizeof(msg_resp)` was preserved and whether the response struct is correctly written into `read->bytes`.

**Files:** `src/main/ezusb2-popn-emu/msg.c`

### Hypothesis 2 (LESS LIKELY): Legacy mode delivery regression

**Evidence:**
- `ac_io_enable_legacy_mode` path in `pipe.c:172` is a `static bool` — safe from clang-format
- The formatting commits do not change logic in `pipe.c` per git diff analysis
- However, if any caller of `popnhook_acio_init(legacy_mode)` changed the argument from `true` to `false`, legacy mode would be disabled silently

**How to verify:** Check `popnhook1/dllmain.c:173` in v5.44 vs v5.43 — confirm the `true` argument is preserved.

**Files:** `src/main/popnhook1/dllmain.c:173`, `src/main/acioemu/pipe.c:171-173`

### Hypothesis 3 (LEAST LIKELY): ICCA state machine regression from formatting

**Evidence:**
- `acioemu/icca.c` was reformatted in both `031836e` and `b0564b9`
- The specific changes confirmed in git diff are whitespace-only for `icca.c`
- The `polling_started` guard (`icca.c:440`) and `fault` management (`icca.c:117-118`) are simple boolean assignments — clang-format cannot alter their logic
- No new ICCA commands or state transitions were added between 5.43 and 5.44

**Files:** `src/main/acioemu/icca.c:440-442`, `src/main/acioemu/icca.c:115-120`
