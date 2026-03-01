# BIO2 Protocol Reference

Reverse-engineered protocol reference for BIO2, the I/O board used by modern
IIDX cabinets (IIDX 20 tricoro onward). Derived from `bio2emu/emu.c`,
`bio2emu-iidx/bi2a.c`, `bio2/bi2a-iidx.h`, `bio2/bio2.h`, and
`acioemu/addr.c`. Confidence tags mark the source of each fact:

- `<!-- [Verified] -->` — code and at least one external source agree
- `<!-- [Corroborated] -->` — code matches at least one external source
- `<!-- [Inferred] -->` — code only, no external confirmation

## 1. Overview

<!-- [Inferred] -->

BIO2 is an I/O board used by beatmania IIDX starting from version 20 (tricoro).
It replaces the older ezusb-based I/O used in IIDX 9–19. The physical transport
is USB-to-serial (CDC/ACM class), exposing a virtual COM port to the game.

BIO2 uses ACIO framing over this USB-serial interface. The game sends ACIO
command messages to the BIO2 board; the board responds with input state and
accepts output state (lights, LED segments). See `doc/protocol/acio.md` for
the wire format (framing, escape encoding, checksum, address assignment). This
document covers only BIO2-specific details that are not already described there.

Key differences from standard ACIO deployments:

- BIO2 is a **single-node bus**: exactly one device at bus address 1.
- The node type is `BI2A` (`AC_IO_NODE_TYPE_BI2A = 0x0D060000`).
- BIO2-specific poll commands (`0x0152`, `0x0153`) carry the full 46-byte IIDX
  input state and receive a 48-byte light output state in the same transaction.

## 2. Device Architecture

<!-- [Inferred] -->

From the game's perspective, BIO2 appears as a single COM port. The game opens
the port and sends ACIO frames to it.

On the emulator side, `struct bio2emu_port` wraps an `ac_io_emu` instance
together with a game-specific dispatcher function pointer:

```c
struct bio2emu_port {
    struct ac_io_emu acio;       /* ACIO framing layer */
    const char *port;            /* COM port name (narrow) */
    const wchar_t *wport;        /* COM port name (wide) */
    bio2_bi2a_dispatcher dispatcher; /* game-specific command handler */
};
```

The `bio2emu_port_dispatch_irp()` function intercepts serial IRP calls, feeds
them through the ACIO parser, and routes decoded messages by address:

- Address `0` → `ac_io_emu_cmd_assign_addrs(emu, msg, 1)` (single node)
- Address `1` → `selected_emu->dispatcher(selected_emu, msg)` (BI2A handler)
- `AC_IO_BROADCAST` → logged as unexpected; not used in normal operation

For IIDX, the dispatcher is `bio2_emu_bi2a_dispatch_request`.

## 3. Address Assignment

<!-- [Verified] -->

Address assignment follows the standard ACIO sequence described in
`doc/protocol/acio.md` Section 8. BIO2 has a single node, so the emulator
always calls `ac_io_emu_cmd_assign_addrs` with `node_count = 1`.

The exchange:

```
Host → BIO2:
  AA  00  00 01  00  00  01
  SOF addr ASSIGN_ADDRS seq nbytes checksum

BIO2 → Host (1 node found):
  AA  00  00 01  00  01  01  03
  SOF addr code  seq nbytes count checksum
  (0x00+0x00+0x01+0x00+0x01+0x01 = 0x03)
```

After this exchange, all subsequent commands go to address `0x01`.

## 4. BI2A Command Set

<!-- [Inferred] -->

All commands are unicast to node address `0x01`. Command codes are big-endian
on the wire (`ac_io_u16()` for host-side access). The BI2A command set includes
the standard ACIO commands plus BIO2-specific poll commands.

### Standard ACIO Commands

These follow the same semantics described in `doc/protocol/acio.md` Section 11
and are handled identically across all ACIO node types.

#### GET_VERSION (0x0002)

Request: zero payload (`nbytes = 0`).

Response payload: `struct ac_io_version` (36 bytes).

BI2A-specific field values:

| Field          | Value                                |
| -------------- | ------------------------------------ |
| `type`         | `0x0D060000` (`AC_IO_NODE_TYPE_BI2A`) |
| `flag`         | `0x00`                               |
| `major`        | `0x04`                               |
| `minor`        | `0x00`                               |
| `revision`     | `0x04`                               |
| `product_code` | `"BI2A"`                             |
| `date`         | Compile-time `__DATE__` string       |
| `time`         | Compile-time `__TIME__` string       |

#### START_UP (0x0003)

Request: zero payload.

Response: 1-byte status `0x00`.

#### KEEPALIVE (0x0080)

Request: zero payload.

Response: empty (`nbytes = 0`). Used as a heartbeat.

### BIO2-Specific Commands

These are defined in `src/main/bio2/bio2.h` as `enum bio2_bi2a_cmd`.

#### INIT (0x0100)

<!-- [Inferred] -->

Request: carries initialization parameters (content not fully documented; the
emulator does not inspect the payload).

Response: 1-byte status `0x00`.

The comment in `bio2.h` notes this command is the same code as the standard
`AC_IO_CMD_CLEAR` but with a parameter required to initialize the I/O board
correctly. The emulator treats it uniformly: send status `0x00`.

#### WATCHDOG (0x0120)

<!-- [Inferred] -->

Request: zero or unknown payload (emulator does not inspect payload).

Response: 1-byte status `0x00`.

Sent periodically by the game as a hardware watchdog reset. Real BIO2 hardware
would reset or halt if this message stops arriving within a timeout window.

#### POLL (0x0152)

<!-- [Inferred] -->

The main bidirectional I/O poll for IIDX. See Section 5 (Poll State Structs)
and Section 6 (Poll Behavior) for full details.

Request payload: `struct bi2a_iidx_state_out` (48 bytes) — light output state
from the game.

Response payload: `struct bi2a_iidx_state_in` (46 bytes) — input state from
the I/O board.

#### POLL_WD (0x0153)

<!-- [Inferred] -->

Identical behavior to POLL (0x0152). Both command codes dispatch to the same
handler (`bio2_emu_bi2a_send_state`). The `_WD` suffix likely indicates a
poll-with-watchdog-reset variant — the hardware may combine the watchdog reset
with the poll to reduce bus traffic.

Other BIO2 command codes defined in `bio2.h` but not handled by the IIDX
dispatcher (`bio2_emu_bi2a_dispatch_request`) are not used for IIDX:

| Code     | Name                          | Notes                            |
| -------- | ----------------------------- | -------------------------------- |
| `0x0112` | `BIO2_BI2A_CMD_ACIO_POLL_NOWD` | For libacio-based BIO2 games   |
| `0x0113` | `BIO2_BI2A_CMD_ACIO_POLL_WD`   | For libacio-based BIO2 games   |
| `0x0140` | `BIO2_BI2A_CMD_TAPELED_INIT`   | Tape LED initialization        |
| `0x0141` | `BIO2_BI2A_CMD_TAPELED_SEND`   | Tape LED data                  |

Unknown commands log a warning at level `log_warning` and produce no response.

## 5. Poll State Structs

<!-- [Verified] -->

The POLL/POLL_WD transaction is a combined output+input exchange. The request
body carries the light output state from the game; the response body carries
the input state from the I/O board. Both structs are packed (`#pragma pack(1)`).

Static assertions in `src/main/bio2/bi2a-iidx.h` enforce the sizes:
- `sizeof(struct bi2a_iidx_state_in) == 46`
- `sizeof(struct bi2a_iidx_state_out) == 48`

### 5.1 Output State (Game → BIO2): `struct bi2a_iidx_state_out` (48 bytes)

Sent in the POLL/POLL_WD request body. Carries light and display output.

| Offset | Field         | Size (bytes) | Description                                  |
| ------ | ------------- | ------------ | -------------------------------------------- |
| 0      | `UNK1[3]`     | 3            | 3 light structs — purpose unknown            |
| 3      | `PANEL[4]`    | 4            | Panel button backlight: P1 Start, P2 Start, VEFX, EFFECT |
| 7      | `DECKSW[14]`  | 14           | Deck key lights: P1 1–7, P2 1–7             |
| 21     | `UNK2[2]`     | 2            | Unknown                                      |
| 23     | `SEG16[9]`    | 9            | 16-segment display data (9 characters)       |
| 32     | `SPOTLIGHT1[4]` | 4          | Right top spotlight: Blue, Green, Yellow, Red |
| 36     | `NEONLAMP`    | 1            | Neon accent lamp                             |
| 37     | `SPOTLIGHT2[4]` | 4          | Left top spotlight: Blue, Green, Yellow, Red |
| 41     | `UNK3[7]`     | 7            | Unknown                                      |

Each light element is `struct bi2a_iidx_light` (1 byte, packed):

```c
struct bi2a_iidx_light {
    uint8_t l_unk   : 7;  /* bits 6:0 — unknown/unused */
    uint8_t l_state : 1;  /* bit 7 — 1 = on, 0 = off */
};
```

The emulator reads `l_state` from each light struct and packs the results
into the iidxio API calls before polling inputs:

- `PANEL[0..3]` → `iidx_io_ep1_set_panel_lights(panel_lights)`, bits 0..3
- `DECKSW[0..13]` → `iidx_io_ep1_set_deck_lights(deck_lights)`, bits 0..13
- `SPOTLIGHT1[0..3]` → top lamps bits 0..3 via `iidx_io_ep1_set_top_lamps`
- `SPOTLIGHT2[0..3]` → top lamps bits 4..7 via `iidx_io_ep1_set_top_lamps`
- `NEONLAMP.l_state` → `iidx_io_ep1_set_top_neons(bool)`
- `SEG16[0..8]` → `iidx_io_ep3_write_16seg(const char *)` (9-char display)

Spotlight color ordering (from `enum bi2a_iidx_spotlight_left/right`):

| SPOTLIGHT1 (right) index | Color  |
| ------------------------ | ------ |
| 0                        | Red    |
| 1                        | Yellow |
| 2                        | Green  |
| 3                        | Blue   |

| SPOTLIGHT2 (left) index | Color  |
| ----------------------- | ------ |
| 0                        | Blue   |
| 1                        | Green  |
| 2                        | Yellow |
| 3                        | Red    |

### 5.2 Input State (BIO2 → Game): `struct bi2a_iidx_state_in` (46 bytes)

Sent in the POLL/POLL_WD response body. Carries all input state.

| Offset | Field        | Type                    | Description                           |
| ------ | ------------ | ----------------------- | ------------------------------------- |
| 0      | `SLIDER1`    | `bi2a_iidx_slider`      | VEFX slider 1 (4-bit value, bits 3:0) |
| 1      | `SYSTEM`     | `bi2a_iidx_system`      | System inputs (coin, service, test)   |
| 2      | `SLIDER2`    | `bi2a_iidx_slider`      | VEFX slider 2                         |
| 3      | `UNK1`       | `uint8_t`               | Unknown                               |
| 4      | `SLIDER3`    | `bi2a_iidx_slider`      | VEFX slider 3                         |
| 5      | `UNK2`       | `uint8_t`               | Unknown                               |
| 6      | `SLIDER4`    | `bi2a_iidx_slider`      | VEFX slider 4                         |
| 7      | `SLIDER5`    | `bi2a_iidx_slider`      | VEFX slider 5                         |
| 8      | `coins`      | `uint8_t`               | Coin counter (accumulated count)      |
| 9      | `PANEL`      | `bi2a_iidx_panel`       | Panel button state                    |
| 10     | `UNK4..9`    | `uint8_t[6]`            | Unknown                               |
| 16     | `TURNTABLE1` | `uint8_t`               | Player 1 turntable position           |
| 17     | `TURNTABLE2` | `uint8_t`               | Player 2 turntable position           |
| 18     | `P1SW1`      | `bi2a_iidx_button`      | P1 key 1 (bit 7 = pressed)           |
| 19     | `UNK11`      | `uint8_t`               | Unknown                               |
| 20     | `P1SW2`      | `bi2a_iidx_button`      | P1 key 2                              |
| 21     | `UNK12`      | `uint8_t`               | Unknown (pattern continues)           |
| 22..31 | `P1SW3..P1SW7` | (alternating)         | P1 keys 3–7, each interleaved with UNK |
| 32     | `P2SW1`      | `bi2a_iidx_button`      | P2 key 1                              |
| 33     | `UNK21`      | `uint8_t`               | Unknown                               |
| 34..45 | `P2SW2..P2SW7` | (alternating)         | P2 keys 2–7, each interleaved with UNK |

Sub-struct bit layouts:

```c
struct bi2a_iidx_slider {
    uint8_t s_unk : 4;  /* bits 3:0 — unknown */
    uint8_t s_val : 4;  /* bits 7:4 — slider value 0..15 */
};

struct bi2a_iidx_system {
    uint8_t v_unk1    : 1;  /* bit 0 */
    uint8_t v_coin    : 1;  /* bit 1 — coin input active */
    uint8_t v_service : 1;  /* bit 2 — service button */
    uint8_t v_test    : 1;  /* bit 3 — test button */
    uint8_t v_unk2    : 4;  /* bits 7:4 */
};

struct bi2a_iidx_panel {
    uint8_t y_unk    : 4;  /* bits 3:0 — unknown */
    uint8_t y_effect : 1;  /* bit 4 — EFFECT button */
    uint8_t y_vefx   : 1;  /* bit 5 — VEFX button */
    uint8_t y_start2 : 1;  /* bit 6 — P2 Start button */
    uint8_t y_start1 : 1;  /* bit 7 — P1 Start button */
};

struct bi2a_iidx_button {
    uint8_t b_unk : 7;  /* bits 6:0 — unknown */
    uint8_t b_val : 1;  /* bit 7 — 1 = pressed */
};
```

Panel button bit positions correspond to `enum bi2a_iidx_panel_button`:

| Enum value                          | Field in PANEL | iidxio constant         |
| ----------------------------------- | -------------- | ----------------------- |
| `BI2A_IIDX_PANEL_BUTTON_START_P1`  | `y_start1`     | `IIDX_IO_PANEL_P1_START` |
| `BI2A_IIDX_PANEL_BUTTON_START_P2`  | `y_start2`     | `IIDX_IO_PANEL_P2_START` |
| `BI2A_IIDX_PANEL_BUTTON_VEFX`      | `y_vefx`       | `IIDX_IO_PANEL_VEFX`    |
| `BI2A_IIDX_PANEL_BUTTON_EFFECT`    | `y_effect`     | `IIDX_IO_PANEL_EFFECT`  |

Key button bit positions (from `IIDX_IO_KEY_P1_1..IIDX_IO_KEY_P2_7`):

| `b_val` field | iidxio key bit                    |
| ------------- | --------------------------------- |
| `P1SW1.b_val` | `IIDX_IO_KEY_P1_1`               |
| `P1SW2.b_val` | `IIDX_IO_KEY_P1_2`               |
| ...           | ...                               |
| `P2SW7.b_val` | `IIDX_IO_KEY_P2_7`               |

## 6. Poll Behavior

<!-- [Inferred] -->

The POLL handler (`bio2_emu_bi2a_send_state`) performs the following operations
in order on each call:

### 6.1 Light Output Processing

The 48-byte request body (`struct bi2a_iidx_state_out`) is read:

1. Iterate `PANEL[0..3]`, pack `l_state` bits into a `uint8_t` → call
   `iidx_io_ep1_set_panel_lights(panel_lights)`.
2. Iterate `DECKSW[0..13]`, pack `l_state` bits into a `uint16_t` → call
   `iidx_io_ep1_set_deck_lights(deck_lights)`.
3. Iterate `SPOTLIGHT1[0..3]` into top_lamps bits 0..3 and `SPOTLIGHT2[0..3]`
   into bits 4..7 → call `iidx_io_ep1_set_top_lamps(top_lamps)`.
4. `NEONLAMP.l_state` → call `iidx_io_ep1_set_top_neons(bool)`.
5. `iidx_io_ep1_send()` — commits the light output. If this fails, the
   handler sends status `0x00` instead of the normal state response.
6. `iidx_io_ep3_write_16seg(req_bi2a->SEG16)` — writes the 9-byte 16-segment
   display string. If this fails, sends status `0x00`.

### 6.2 Poll Limiter

<!-- [Inferred] -->

IIDX 25 (Cannon Ballers) polls BIO2 at an extremely high rate. To prevent
CPU saturation and emulate realistic hardware timing, the emulator inserts a
`Sleep(1)` (1 ms sleep) between the light output phase and the input read:

```c
if (poll_delay) {
    Sleep(1);
}
```

`poll_delay` is initialized from the `disable_poll_limiter` argument to
`bio2_emu_bi2a_init()`:

```c
poll_delay = !disable_poll_limiter;
```

Passing `disable_poll_limiter = true` skips the sleep, which is necessary
in unit tests to prevent test timeouts.

### 6.3 Input Read Sequence

After the light output and optional sleep:

1. `iidx_io_ep2_recv()` — triggers an input poll from the I/O device. On
   failure, sends status `0x00`.
2. Read turntable, sliders, keys, panel, and sys from the iidxio API.
3. Construct `struct bi2a_iidx_state_in` response body.
4. Push response via `ac_io_emu_response_push(emu, &resp, 0)`.

### 6.4 Coin Latch (Edge Detection)

<!-- [Inferred] -->

The SYSTEM.v_coin bit from `iidx_io_ep2_get_sys()` is subject to edge
detection. The emulator maintains a `coin_latch` flag and a `coin_count`
accumulator:

```c
if (body->SYSTEM.v_coin) {
    if (!coin_latch) {
        coin_latch = true;
        coin_count += 1;
    }
} else {
    coin_latch = false;
}
body->coins = coin_count;
```

Behavior:
- Rising edge (coin bit transitions from 0 to 1): `coin_count` increments by 1,
  `coin_latch` set to `true`.
- Held high: `coin_latch` is already `true`, count does not increment again.
- Falling edge (coin bit transitions from 1 to 0): `coin_latch` cleared to
  `false`, ready to detect the next coin insertion.

The `coins` field in the response accumulates monotonically across polls. The
game uses the delta between successive `coins` values to detect new credits.

The `coin_latch` and `coin_count` variables are module-level statics, shared
across all instances. This works correctly because BIO2 is a single-port
setup.

### 6.5 Slider Defaults from vefx.txt

<!-- [Inferred] -->

At initialization, the emulator attempts to open a file named `vefx.txt` in
the current working directory. If found, it reads 5 integer values representing
default slider positions (range 0..15 each):

```c
fscanf(f, "%d %d %d %d %d",
    &default_sliders[0], &default_sliders[1], &default_sliders[2],
    &default_sliders[3], &default_sliders[4]);
```

If a slider's default value is in the valid range (0–15), it is used instead
of the live value from `iidx_io_ep2_get_slider()`. This allows operators to
lock slider positions (e.g., for games that require specific effect settings).

Values outside 0–15 are treated as "not configured" and the live slider value
is used instead.

## 7. Turntable Accumulator

<!-- [Inferred] -->

The turntable output in the response struct is either a pass-through of the raw
`uint8_t` position from `iidx_io_ep2_get_turntable(player_no)`, or an
accumulated value with optional multiplier scaling.

When a turntable multiplier is configured via
`bio2_emu_bi2a_set_tt_multiplier(float multiplier)`, the emulator switches to
accumulator mode:

```c
static bool tt_multiplier_set;
static float tt_multiplier = 1.f;
static uint8_t tt_accum[2];
static int16_t tt_last[2];
```

Per poll, for each turntable:

1. Read current raw position from `iidx_io_ep2_get_turntable(tt_no)` as
   `int16_t current_tt`.
2. Compute delta = `get_wrapped_delta_s16(current_tt, tt_last[tt_no], 256)`.
   This function handles 8-bit wraparound (0–255 wraps at both ends).
3. Scale: `scaled_delta = round(delta * tt_multiplier)`.
4. Accumulate: `tt_accum[tt_no] += scaled_delta` (uint8_t wraparound).
5. Write `body->TURNTABLE1 = tt_accum[0]`, `body->TURNTABLE2 = tt_accum[1]`.

When no multiplier is set (`tt_multiplier_set == false`), the raw turntable
position is passed through directly without accumulation.

The turntable position is an 8-bit unsigned value (0–255), representing angular
position with wraparound. The emulator's `get_wrapped_delta_s16` handles
forward and backward rotation correctly across the 0/255 boundary.

## 8. Timing Considerations

<!-- [Inferred] -->

**Poll rate:** IIDX 25+ polls BIO2 extremely rapidly (approaching USB full-speed
limits). Without the poll limiter, this can saturate a CPU core. The poll
limiter (`Sleep(1)`) caps the effective rate at approximately 1000 Hz.

**Response delay:** The emulator pushes responses with `delay_us = 0`, meaning
responses are immediately available in the `ac_io_in` queue. There is no
simulated hardware latency beyond the poll limiter sleep.

**Drain behavior:** `ac_io_in_drain()` dequeues responses into the read buffer.
Responses with `delay_us = 0` are always available immediately (the
`time_get_elapsed_us` check returns a large value when using `time-stub` with
default 1s elapsed). Legacy mode is not used for BIO2 — all queued responses
drain in a single call.
