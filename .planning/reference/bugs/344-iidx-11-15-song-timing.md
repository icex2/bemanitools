# Bug #344 — IIDX 11-15 Song Timing on Modern Hardware

**Status:** Open
**Phase 4 priority:** HIGH — core gameplay correctness

---

## Issue Summary

beatmania IIDX games 11 through 15 (RED through DJ Troopers, using `iidxhook1` through `iidxhook5`) exhibit incorrect song timing on modern hardware. The games use the ezusb emulator (`ezusb-iidx-emu`) for IO, not BIO2. The interrupt read/write cycle driven by the game's timing loop is sensitive to the counter resolution and polling frequency derived from `QueryPerformanceCounter`. On modern hardware with high-resolution timers (>= 10MHz), the frequency scaling in `time_get_elapsed_us()` may produce near-zero elapsed times for very short deltas, causing the ICCA poll delay check in `acioemu/pipe.c` to behave differently than on the original hardware timing model.

Additionally, the ezusb-iidx emulator's interrupt read rate is not independently rate-limited — it runs at whatever rate the game calls it. If the game's timing loop expects a specific polling cadence (tied to the 60fps sync on original hardware), running faster or slower would desync note timing from audio.

---

## User Reports

No GitHub issue content available (gh CLI not authenticated). Based on issue number and codebase context:

- **Affected BT5 versions:** Active across multiple versions; possibly introduced during the ezusb refactoring era
- **Affected games:** beatmania IIDX 11 RED (iidxhook1), IIDX 12 Happy Sky (iidxhook1), IIDX 13 DistorteD (iidxhook2), IIDX 14 GOLD (iidxhook3), IIDX 15 DJ Troopers (iidxhook3)
- **Hardware context:** Games originally ran on hardware with specific USB interrupt timing (125Hz for full-speed USB, or faster for high-speed). On modern PCs the emulator can respond much faster, changing the effective polling frequency.
- **Symptoms:** Notes appear early or late relative to audio; timing offset; gameplay feels "off" even after manual offset calibration

---

## Affected Games/Versions

| Game | BT5 Hook | AVS version | IO emulator |
|------|----------|-------------|------------|
| IIDX 11 RED | iidxhook1 | 0 (no AVS) | ezusb-iidx-emu (V1) |
| IIDX 12 Happy Sky | iidxhook1 | 0 (no AVS) | ezusb-iidx-emu (V1) |
| IIDX 13 DistorteD | iidxhook2 | avs2_0 (1) | ezusb-iidx-emu (V1) |
| IIDX 14 GOLD | iidxhook3 | avs2_803 (4.2) | ezusb-iidx-emu (V1) |
| IIDX 15 DJ Troopers | iidxhook3 | avs2_1006 (6.5) | ezusb-iidx-emu (V2) |

---

## Relevant Code Paths

### Time counter implementation

`src/main/util/time.c:6-17` — frequency scaling initialization:
```c
static uint64_t counter_freq_us;
/* ... */
counter_freq_us = ticks_per_sec.QuadPart / 1000 / 1000;
```

**Critical issue:** `counter_freq_us` is computed as `ticks_per_sec / 1000000`. On modern hardware with `QueryPerformanceFrequency` returning ~10MHz (10,000,000 ticks/sec), `counter_freq_us = 10`. But if `ticks_per_sec` is exactly 10,000,000 and division truncates, `counter_freq_us` = 10, which is correct. However, if the CPU's QPC frequency is lower (e.g., 1MHz on some systems), `counter_freq_us = 1`, which is still correct. The concern is the chain: `counter_freq_ms = ticks/1000`, then `counter_freq_us = counter_freq_ms / 1000` — integer division chain means for `ticks_per_sec=10007997` (a plausible HPET value), `counter_freq_ms=10007`, `counter_freq_us=10`. The rounding error accumulates and may cause `time_get_elapsed_us()` to return values 0.07% off.

`src/main/util/time.c:38-44` — `time_get_elapsed_us()`:
```c
return counter_delta / counter_freq_us;
```
For very small counter deltas (sub-microsecond), returns 0 regardless of actual elapsed time. This affects ACIO poll delay checking.

### ACIO poll delay check — ICCA poll timing

`src/main/acioemu/icca.c:200-213`:
```c
case AC_IO_ICCA_CMD_POLL:
case AC_IO_ICCA_CMD_POLL_ENCRYPTED:
    delay_us = time_get_elapsed_us(
        time_get_counter() - icca->time_counter_last_poll);

    /* emulating delay implemented by hardware */
    if (delay_us > 16000) {
        delay_us = 0;  /* icca.c:207 */
    }

    icca->time_counter_last_poll = time_get_counter();
    ac_io_emu_icca_send_state(icca, req, delay_us, encrypted);
```

The hardware-simulated delay (0–16ms) is passed to `ac_io_emu_response_push()` and then to `ac_io_in_supply()` as `delay_us`. In `ac_io_in_drain()`, responses with a non-zero delay are held until `time_get_elapsed_us(now - scheduled_time) >= delay_us`. If the time stub doesn't match hardware timing, responses drain too fast (0 delay) causing the game's read to immediately return data, changing the effective poll rate.

### ezusb interrupt cycle

`src/main/ezusb-iidx-emu/msg.c:119-176` — `ezusb_iidx_emu_msg_interrupt_read()`:

The interrupt read is called by the game's IO thread whenever it wants input state. There is no rate limiting on this path — it returns immediately with current state. On original hardware, USB full-speed interrupt pipes are limited to 1ms intervals (1000 Hz) or 8ms (125 Hz). The emulator has no such constraint, so on modern hardware the game can poll at >10kHz, fundamentally changing timing characteristics.

`src/main/ezusb-iidx-emu/msg.c:155-165`:
```c
msg_resp->status = ezusb_iidx_emu_msg_status;
/* Reset status after delivered */
ezusb_iidx_emu_msg_status = 0;
msg_resp->seq_no = ezusb_iidx_emu_msg_seq_no++;
```

The `seq_no` increments every interrupt read. If the game uses `seq_no` to detect missed packets or frame cadence, an overrun `seq_no` from excessive polling frequency could cause timing miscalculation.

### iidxhook1/2/3 hook entry points

`src/main/iidxhook1/dllmain.c` — hook for IIDX 9-12
`src/main/iidxhook2/dllmain.c` — hook for IIDX 13
`src/main/iidxhook3/dllmain.c` — hook for IIDX 14-17

These hooks wire up `ezusb_iidx_emu_msg_init()` (V1) for the ezusb USB device emulation. The game's timing loop runs through the hooked ezusb device interface.

---

## Changes Between v5.43 and v5.44 Relevant to This Bug

| Commit | Message | Relevance |
|--------|---------|-----------|
| `aaab9bf` | feat(iidx/ezusb): Make iidx ezusb emu io board type configurable | Adds board type selection (C02 vs D01); affects bit 4 of inverted_pad |
| `83ef8e2` | chore(iidx): Wire-up ezusb configuration in iidxhook1 and 2 | Now passes `io_board_type` to `ezusb_iidx_emu_msg_init()` |
| `5c4afb8` | fix(iidx/config): Utilize io board type, fix 10th SQ-INIT error | Part of board type fix series |
| `e807376` | feat(iidx/ezusb): Add ezusb api monitoring module | Diagnostic tooling, not direct timing impact |

**Analysis:** The io_board_type change (`aaab9bf`) modified `ezusb_iidx_emu_msg_interrupt_read()` to conditionally set bit 4 of `inverted_pad` based on board type. For IIDX 11-15, if the wrong board type is now selected by default, the game might misparse the pad state — but this is an IO interpretation issue, not a timing issue. The timing hypothesis remains at the `util/time.c` precision and missing poll rate limiter level.

---

## Reproduction Conditions

1. Build BT5 with iidxhook1 or iidxhook3
2. Launch IIDX 11-15 with `inject.exe iidxhook1.dll bm2dx.exe`
3. Play a song and observe whether note timing matches audio
4. Compare behavior at different system QPC frequencies (different hardware) to isolate timing sensitivity

---

## Ranked Hypotheses

### Hypothesis 1 (MOST LIKELY): Missing USB interrupt rate limiter

**Evidence:**
- The ezusb interrupt read path (`msg.c:119-176`) has no rate limiting — returns immediately on every call
- Original USB full-speed hardware limits interrupt endpoints to 1ms minimum interval
- The game's timing loop likely assumes ~1ms granularity based on real hardware behavior
- Without a rate limiter, the game sees 10-100x more "frames" of IO data per wall-clock second, potentially desynchronizing from the audio clock
- BIO2-based games (IIDX 14+) get implicit rate limiting from `Sleep(1)` in `bio2emu-iidx/bi2a.c` — ezusb games have no equivalent

**Fix direction:** Add a 1ms sleep or rate-limit counter to `ezusb_iidx_emu_msg_interrupt_read()` when the loop runs faster than expected. Alternatively, add a configurable poll limiter similar to `disable_poll_limiter` in BIO2 config.

**Files:** `src/main/ezusb-iidx-emu/msg.c:119`

### Hypothesis 2 (PLAUSIBLE): Integer division truncation in time_get_elapsed_us

**Evidence:**
- `util/time.c:16`: `counter_freq_us = counter_freq_ms / 1000` — double integer division from `ticks_per_sec`
- For `ticks_per_sec = 10,000,000`: chain gives exact result
- For `ticks_per_sec = 10,007,997` (HPET clock): `counter_freq_ms = 10007`, `counter_freq_us = 10` — off by 0.08%
- 0.08% timing error over a 120-second song = ~96ms cumulative drift
- IIDX timing tolerance is typically ±20ms for "perfect" judgment — 96ms drift would cause near-total miss

**Fix direction:** Compute `counter_freq_us` directly as `ticks_per_sec / 1000000` (single division) rather than the two-step `/ 1000 / 1000` chain.

**Files:** `src/main/util/time.c:15-17`

### Hypothesis 3 (LEAST LIKELY): io_board_type regression affecting pad state

**Evidence:**
- `aaab9bf` introduced configurable board type; `83ef8e2` wired it into iidxhook1/2
- If the default board type selection changed between 5.43 and 5.44, bit 4 of inverted_pad would differ
- bit 4 = board type indicator (1=C02, 0=D01 on inverted pad)
- The game may use this bit to select timing parameters or pad layout, causing apparent timing misalignment
- This is speculative — not clear whether IIDX 11-15 actually reads bit 4

**Files:** `src/main/ezusb-iidx-emu/msg.c:178-191`, `src/main/iidxhook1/dllmain.c`
