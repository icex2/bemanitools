# ezusb-iidx Protocol Reference

Reverse-engineering reference for the ezusb-iidx I/O protocol used by
beatmania IIDX 9th Style through 19 Lincle. Each behavior is tagged:

- **Verified** — directly confirmed by emulator source code
- **Corroborated** — consistent across multiple source files or structs
- **Inferred** — reasonable interpretation; may have alternatives

---

## 1. Overview

The ezusb-iidx board is based on a Cypress EZ-USB FX2 microcontroller. It is
the I/O board used by beatmania IIDX from 9th Style (IIDX 9) through 19 Lincle
(IIDX 19). This protocol is completely separate from ACIO and BIO2:

- Different physical USB device with its own VID/PID **[Inferred]**
- Different packet format: fixed-size interrupt packets + 64-byte bulk packets
- No ACIO SOF framing, no ACIO checksum, no ACIO address dispatch

Two hardware revisions of the board exist: C02 and D01. They use different FPGA
firmware and differ in one bit of the interrupt read packet (see Section 11).
**[Verified]**

Starting with IIDX 14 (Gold), the firmware was updated to a V2 node set that
removed the serial card reader (node 0x04 changed role from SERIAL to FPGA_V2).
**[Verified — msg.c node table comments]**

---

## 2. USB Pipe Architecture

Four pipes are used: **[Verified — `ezusb_iidx_msg_pipe` enum]**

| Pipe | Direction | Type | Purpose |
|------|-----------|------|---------|
| 0 | Host → Device | Interrupt OUT | Light control + node command dispatch |
| 1 | Device → Host | Interrupt IN | Pad state + status response |
| 2 | Host → Device | Bulk OUT | Node data write (FPGA firmware, EEPROM, etc.) |
| 3 | Device → Host | Bulk IN | Node data read |

Each game poll cycle sends one interrupt write, reads one interrupt response,
then optionally exchanges bulk packets if the current node needs data transfer.
**[Inferred from dispatch logic in msg.c]**

---

## 3. Interrupt Write Packet

Host → device, fixed size. Carries light control outputs and the current node
command. **[Verified — `struct ezusb_iidx_msg_interrupt_write_packet`]**

```
Offset  Size  Field          Description
0       2     deck_lights    Key lighting bitmap, P1 and P2 keys
2       1     node           Node ID to address for this cycle
3       1     cmd            Command byte for the addressed node
4       2     cmd_detail     Two bytes of command-specific data
6       1     panel_lights   Panel button light bitmap
7       1     unk0           Unknown (observed zero in normal operation)
8       1     top_lamps      Top lamp bitmap (speaker/corner lamps)
9       1     top_neons      Neon/cabinet lighting control
10      1     fpga_run       Must be 1 after FPGA flash; 0 = FPGA output frozen
11      1     unk2           Unknown
12      1     unk3           Unknown
13      1     unk4           Unknown
14      1     unk5           Unknown
15      1     unk6           Unknown
```

**Verified:** `fpga_run` comment in msg.h: "Ensure this is always enabled (1)
after flashing the fpga prog. Otherwise, all data coming from the fpga is never
updated."

The `node` field selects which node receives the `cmd` and `cmd_detail` bytes.
The emulator records this as `cur_node` for the subsequent bulk read dispatch.
**[Verified — msg.c]**

---

## 4. Interrupt Read Packet

Device → host, fixed size. Carries pad state, status from previous command, and
turntable/slider data. **[Verified — `struct ezusb_iidx_msg_interrupt_read_packet`]**

```
Offset  Size  Field                 Description
0       4     inverted_pad          Button/pad state, active-low (see 4.1)
4       1     status                Status byte from last node command (cleared after read)
5       1     unk0                  Unknown
6       1     unk1                  Unknown
7       1     p2_turntable          Player 2 turntable position (raw encoder value)
8       1     p1_turntable          Player 1 turntable position (raw encoder value)
9       1     seq_no                Monotonically incrementing sequence counter
10      1     fpga2_check_flag_unkn Must be 2 after FPGA V2 CHECK command; triggers game validation
11      1     fpga_write_ready      1 = ready for next FPGA bulk write page; 0 = busy
12      1     serial_io_busy_flag   Bit 0 = read buffer busy, bit 1 = write buffer busy
13      3     sliders               Slider positions (see 4.2)
```

### 4.1 inverted_pad Bit Layout

All bits are active-low: 0 = pressed, 1 = not pressed. **[Verified — emulator
inverts the assembled word: `msg_resp->inverted_pad = ~msg_resp->inverted_pad`]**

Bit positions in the 32-bit word (before inversion): **[Verified — msg.c and msg.h comments]**

| Bit | Input |
|-----|-------|
| 0–3 | Not used (dip switches area) |
| 4   | C02/D01 board type identifier (1 = C02, 0 = D01; active-low logic applies) |
| 5   | Not used |
| 6   | USB mute? (noted in msg.h comment) |
| 7   | Not used |
| 8   | P1 Key 1 |
| 9   | P1 Key 2 |
| 10  | P1 Key 3 |
| 11  | P1 Key 4 |
| 12  | P1 Key 5 |
| 13  | P1 Key 6 |
| 14  | P1 Key 7 |
| 15  | P2 Key 1 |
| 16  | P2 Key 2 |
| 17  | P2 Key 3 |
| 18  | P2 Key 4 |
| 19  | P2 Key 5 |
| 20  | P2 Key 6 |
| 21  | P2 Key 7 |
| 22  | Coin mech (also carries sys bit 2 — coin mode select) |
| 23  | Not used |
| 24  | P1 Start |
| 25  | P2 Start |
| 26  | VEFX button |
| 27  | Effector button |
| 28  | Test button |
| 29  | Service button |
| 30  | Unknown / not used |
| 31  | Coin mode state: 0 = coin mode 1, 1 = coin mode 2 |

Assembly in emulator before inversion (from msg.c): **[Verified]**

```c
msg_resp->inverted_pad =
    ((iidx_io_ep2_get_keys() & 0x3FFF) << 8)   /* P1+P2 keys at bits 8-21 */
    | ((iidx_io_ep2_get_panel() & 0x0F) << 24)  /* panel bits at 24-27 */
    | ((iidx_io_ep2_get_sys() & 0x07) << 28)    /* sys bits at 28-30 */
    | (((iidx_io_ep2_get_sys() >> 2) & 0x01) << 22); /* sys bit 2 -> bit 22 */

/* Coin mode state from coin node, not from iidxio */
msg_resp->inverted_pad &= ~(1 << 31);
if (ezusb_iidx_emu_node_coin_get_mode() == 1) {
    msg_resp->inverted_pad |= (1 << 31);
}

msg_resp->inverted_pad = ~msg_resp->inverted_pad; /* invert: active-high -> active-low */
```

The `keys` value from iidxio is 14 bits (bits 0–13 = P1 key 1–7 then P2 key
1–7). `panel` is 4 bits (P1 Start, P2 Start, VEFX, Effector). `sys` is 3 bits
(Test, Service, unknown). **[Verified — iidxio.h API names and bit layout]**

### 4.2 Slider Packing

Three bytes carry five slider values. Each slider value is 4 bits. **[Verified — msg.c]**

```
sliders[0] = slider[0] | (slider[1] << 4)
sliders[1] = slider[2] | (slider[3] << 4)
sliders[2] = slider[4]
```

### 4.3 Status Byte Lifecycle

The `status` byte holds the return value from the last node `process_cmd` call.
It is reset to 0 after each interrupt read. **[Verified — msg.c: "Reset status
after delivered (important for eeprom reading)"]**

### 4.4 fpga2_check_flag_unkn

This field is always set to 2 in the emulator: **[Verified — msg.c comment]**

> "this needs to be 2 with FPGA2_CMD_CHECK2, otherwise the game's fpga check
> will fail"

It is not conditional on FPGA V2 being active — it is hardcoded to 2 in every
interrupt read response. **[Verified]**

---

## 5. Bulk Packet Format

Both bulk IN and bulk OUT use the same 64-byte packet structure. **[Verified —
`struct ezusb_iidx_msg_bulk_packet`]**

```
Offset  Size  Field    Description
0       1     node     Node ID this packet belongs to
1       1     page     Page/sequence number within multi-packet transfer
2       62    payload  Node-specific data
```

`EZUSB_PAGESIZE` = 62. **[Verified — msg.h]**

On bulk IN, the emulator fills `node = 0x00`, `page = 0x00`, and zeroes the
payload as a stub for nodes that return no meaningful data (e.g., FPGA).
**[Verified — node-fpga.c `ezusb_iidx_emu_node_fpga_read_packet`]**

---

## 6. Node Dispatch

The emulator maintains a 256-entry sparse array indexed by node ID.
**[Verified — msg.c]**

```c
static const struct ezusb_iidx_emu_node *ezusb_iidx_emu_msg_nodes[256];
```

### 6.1 Node Vtable Interface

Each node is a `struct ezusb_iidx_emu_node` (from `ezusb-emu/node.h`):
**[Verified]**

```c
struct ezusb_iidx_emu_node {
    const uint8_t node_id;
    void (*init_node)(void);         /* optional constructor */
    uint8_t (*process_cmd)(uint8_t cmd_id, uint8_t cmd_data, uint8_t cmd_data2);
    bool (*read_packet)(struct ezusb_iidx_msg_bulk_packet *pkg);
    bool (*write_packet)(const struct ezusb_iidx_msg_bulk_packet *pkg);
};
```

### 6.2 Interrupt Write Dispatch

On each interrupt write: **[Verified — msg.c]**

1. Light control outputs are forwarded to `iidx_io_ep1_set_*()`.
2. `msg_req->node` is validated against the node table.
3. `cur_node = msg_req->node` is saved for subsequent bulk reads.
4. `node->process_cmd(cmd, cmd_detail[0], cmd_detail[1])` is called.
5. The return value is stored in `ezusb_iidx_emu_msg_status`.

### 6.3 Bulk Read Dispatch

On each bulk read, the emulator calls `cur_node->read_packet(pkt)` using the
node saved from the last interrupt write. **[Verified — msg.c]**

### 6.4 Bulk Write Dispatch

On bulk write, the emulator dispatches to `pkt->node->write_packet(pkt)` using
the `node` field in the packet itself (not `cur_node`). **[Verified — msg.c]**

### 6.5 Node Tables

V1 node table (IIDX 9–13, `ezusb_iidx_emu_msg_nodes`): **[Verified]**

| Node ID | Constant | Node |
|---------|----------|------|
| 0x00 | `EZUSB_IIDX_MSG_NODE_NONE` | None (no-op) |
| 0x01 | `EZUSB_IIDX_MSG_NODE_SECURITY_PLUG` | Security plug (V1) |
| 0x02 | `EZUSB_IIDX_MSG_NODE_EEPROM` | EEPROM (V1) |
| 0x04 | `EZUSB_IIDX_MSG_NODE_SERIAL` | Serial (mag card reader) |
| 0x05 | `EZUSB_IIDX_MSG_NODE_16SEG` | 16-segment display |
| 0x09 | `EZUSB_IIDX_MSG_NODE_COIN` | Coin counter/mode |
| 0x0C | `EZUSB_IIDX_MSG_NODE_WDT` | Watchdog timer |
| 0x10 | `EZUSB_IIDX_MSG_NODE_FPGA_V1` | FPGA (V1) |
| 0x40 | `EZUSB_IIDX_MSG_NODE_SRAM` | SRAM |
| 0xFE | `EZUSB_IIDX_MSG_NODE_SECURITY_MEM` | Security memory (V1) |

V2 node table (IIDX 14+, `ezusb_iidx_emu_msg_v2_nodes`): **[Verified]**

| Node ID | Constant | Node |
|---------|----------|------|
| 0x00 | `EZUSB_IIDX_MSG_NODE_NONE` | None (no-op) |
| 0x01 | `EZUSB_IIDX_MSG_NODE_SECURITY_PLUG` | Security plug (V2) |
| 0x02 | `EZUSB_IIDX_MSG_NODE_EEPROM` | EEPROM (V2) |
| 0x04 | `EZUSB_IIDX_MSG_NODE_FPGA_V2` | FPGA (V2) — replaces serial |
| 0x05 | `EZUSB_IIDX_MSG_NODE_16SEG` | 16-segment display |
| 0x09 | `EZUSB_IIDX_MSG_NODE_COIN` | Coin counter/mode |
| 0x0C | `EZUSB_IIDX_MSG_NODE_WDT` | Watchdog timer |
| 0x40 | `EZUSB_IIDX_MSG_NODE_SRAM` | SRAM |
| 0xFE | `EZUSB_IIDX_MSG_NODE_SECURITY_MEM` | Security memory (V2) |

Note: FPGA_V1 (0x10) is absent from the V2 table; SERIAL (0x04) is absent from
V2 and replaced by FPGA_V2 (also 0x04). **[Verified — msg.c node arrays]**

---

## 7. FPGA Node V1 (IIDX 9–13)

FPGA_V1 node ID: `0x10`. Handles FPGA firmware upload for C02 and D01 boards
on IIDX 9 through DistorteD (IIDX 13). **[Verified — nodes.c comment]**

### 7.1 Commands

Commands are sent via interrupt write `cmd` field; status is returned in the
next interrupt read `status` field.

| Command | Value | Status on success | Notes |
|---------|-------|-------------------|-------|
| `EZUSB_IIDX_FPGA_CMD_V1_INIT` | 0x01 | `OK_2` (0xFE) | Begin initialization |
| `EZUSB_IIDX_FPGA_CMD_V1_CHECK` | 0xFF | `OK` (0x00) | Reset/check |
| `EZUSB_IIDX_FPGA_CMD_V1_CHECK_2` | 0x02 | `OK_2` (0xFE) | Secondary check |
| `EZUSB_IIDX_FPGA_CMD_V1_WRITE` | 0x03 | `OK` (0x00) | Begin write; cmd_detail = prog size big-endian |
| `EZUSB_IIDX_FPGA_CMD_V1_WRITE_DONE` | 0x04 | `OK_2` (0xFE) | Flash complete |

### 7.2 Status Codes

**[Verified — fpga-cmd.h; code comment: "I even checked the firmware and they
used the same return code for both, error and ok on some calls"]**

| Constant | Value | Meaning |
|----------|-------|---------|
| `EZUSB_IIDX_FPGA_CMD_STATUS_V1_OK` | 0x00 | Success (used by CHECK and WRITE) |
| `EZUSB_IIDX_FPGA_CMD_STATUS_V1_OK_2` | 0xFE | Success (used by INIT, CHECK_2, WRITE_DONE) |
| `EZUSB_IIDX_FPGA_CMD_STATUS_V1_FAULT` | 0xFE | Error — same value as OK_2 |

OK_2 and FAULT share the value 0xFE. The game cannot distinguish success from
failure for INIT, CHECK_2, and WRITE_DONE. **[Verified — constants in fpga-cmd.h]**

### 7.3 Init Sequence

Typical game boot sequence: **[Inferred from command semantics and emulator state machine]**

1. Send INIT (0x01) → expect status 0xFE
2. Send CHECK (0xFF) → expect status 0x00
3. Send CHECK_2 (0x02) → expect status 0xFE
4. Send WRITE (0x03) with `cmd_detail` = firmware size (big-endian uint16)
5. Stream firmware via bulk writes (62 bytes/page)
6. Send WRITE_DONE (0x04) → expect status 0xFE

### 7.4 Firmware Storage

The emulator stores received firmware in a 65535-byte internal buffer
(`ezusb_iidx_emu_node_fpga_mem`). `EZUSB_IIDX_FPGA_CMD_V1_WRITE` resets the
write pointer and fills the buffer with 0xFF before upload. **[Verified — node-fpga.c]**

---

## 8. FPGA Node V2 (IIDX 14+)

FPGA_V2 node ID: `0x04`. Used from Gold (IIDX 14) onwards with the C02 board.
**[Verified — nodes.c comment: "Used on Gold onwards"]**

### 8.1 Commands

| Command | Value | Status on success |
|---------|-------|-------------------|
| `EZUSB_IIDX_FPGA_CMD_V2_INIT` | 0x01 | `INIT_OK` (0x41) |
| `EZUSB_IIDX_FPGA_CMD_V2_CHECK` | 0x02 | `CHECK_OK` (0x42) |
| `EZUSB_IIDX_FPGA_CMD_V2_WRITE` | 0x03 | `WRITE_OK` (0x43) |
| `EZUSB_IIDX_FPGA_CMD_V2_WRITE_DONE` | 0x04 | `WRITE_OK` (0x43) |

### 8.2 Status Codes

**[Verified — fpga-cmd.h]**

| Constant | Value | Meaning |
|----------|-------|---------|
| `EZUSB_IIDX_FPGA_CMD_STATUS_V2_NULL` | 0x00 | No status / initial |
| `EZUSB_IIDX_FPGA_CMD_STATUS_V2_INIT_OK` | 0x41 | INIT succeeded |
| `EZUSB_IIDX_FPGA_CMD_STATUS_V2_CHECK_OK` | 0x42 | CHECK succeeded |
| `EZUSB_IIDX_FPGA_CMD_STATUS_V2_WRITE_OK` | 0x43 | WRITE / WRITE_DONE succeeded |
| `EZUSB_IIDX_FPGA_CMD_STATUS_V2_FAULT` | 0xFE | Error |

Unlike V1, each command in V2 has a distinct success status. The game can
reliably detect initialization failures. **[Corroborated — fpga-cmd.h
constants and node-fpga.c]**

### 8.3 fpga2_check_flag_unkn

After the V2 CHECK command, `fpga2_check_flag_unkn` in the interrupt read packet
must be 2. The emulator hardcodes this field to 2 on every read; the game uses
it to validate the FPGA check result. **[Verified — msg.c comment]**

---

## 9. Serial Node (IIDX 9–13, V1 only)

Serial node ID: `0x04`. Present only in the V1 node table. Handles the magnetic
card reader via an H8 microcontroller sub-protocol on IIDX 9 through HappySky
(IIDX 12). **[Verified — nodes.c comment; removed in V2 table]**

### 9.1 Serial Commands (via interrupt write)

**[Verified — `ezusb_iidx_serial_command` enum in serial-cmd.h]**

| Command | Value | Status OK | Status Fault |
|---------|-------|-----------|--------------|
| `EZUSB_IIDX_SERIAL_CMD_READ_BUFFER` | 0x02 | 0x00 | 0xFE |
| `EZUSB_IIDX_SERIAL_CMD_WRITE_BUFFER` | 0x03 | 0x00 | 0xFE |
| `EZUSB_IIDX_SERIAL_CMD_CLEAR_READ_BUFFER` | 0x04 | 0x00 | 0xFE |
| `EZUSB_IIDX_SERIAL_CMD_CLEAR_WRITE_BUFFER` | 0x05 | 0x00 | 0xFE |

### 9.2 H8 Framing (sub-protocol over serial bulk data)

Data exchanged over bulk packets uses an H8 framing layer: **[Verified — node-serial.c enums]**

| Constant | Value | Meaning |
|----------|-------|---------|
| `CMD_H8_REQ` | 0xAA | Request from host to H8 |
| `CMD_H8_RESP` | 0xA5 | Response from H8 to host |
| `CMD_NODE_REQ` | 0x00 | Node-level request |
| `CMD_NODE_RESP` | 0x01 | Node-level response |

H8 commands: **[Verified — node-serial.c]**

| Command | Value | Meaning |
|---------|-------|---------|
| `H8_CMD_NODE_ENUM` | 0x01 | Enumerate nodes on H8 bus |
| `H8_CMD_GET_VERSION` | 0x02 | Get H8 firmware version |
| `H8_CMD_PROG_EXEC` | 0x03 | Execute program on H8 |

### 9.3 Node-Level Card Reader Commands

**[Verified — `ezusb_iidx_emu_node_serial_node_cmd` enum in node-serial.c]**

| Command | Value | Purpose |
|---------|-------|---------|
| `NODE_CMD_CARD_INIT` | 0x00 | Initialize card reader |
| `NODE_CMD_KEYBOARD_INIT` | 0x10 | Initialize keyboard unit |
| `NODE_CMD_CARD_RW_UNIT_GET_STATUS` | 0x12 | Get card reader unit status |
| `NODE_CMD_CARD_SLOT_SET_STATE` | 0x14 | Open or close card slot |
| `NODE_CMD_CARD_WRITE` | 0x16 | Write card data |
| `NODE_CMD_CARD_READ` | 0x18 | Read card data |
| `NODE_CMD_CARD_FORMAT_COMPLETE` | 0x1E | Signal format completion |
| `NODE_CMD_CARD_GET_STATUS` | 0x20 | Get card insertion status |
| `NODE_CMD_KEYBOARD_GET_STATUS` | 0x24 | Get keyboard status |
| `NODE_CMD_KEYBOARD_READ_DATA` | 0x26 | Read keyboard data |
| `NODE_CMD_KEYBOARD_GET_BUFFER_SIZE` | 0x27 | Get keyboard buffer size |

### 9.4 Card Slot States

**[Verified — `ezusb_iidx_emu_node_serial_card_slot_state` enum in node-serial.c]**

| State | Value | Meaning |
|-------|-------|---------|
| `CARD_SLOT_STATE_CLOSE` | 0 | Slot closed; reject card inserts |
| `CARD_SLOT_STATE_OPEN` | 1 | Slot open; accept next card |

### 9.5 Magnetic Card State Machine

The serial node manages a magnetic stripe card state machine (implemented in
`card-mag.c`, included directly into `node-serial.c`). **[Verified — node-serial.c
`#include "ezusb-iidx-emu/card-mag.c"`]**

Card data format uses a CRC-based checksum. Different game versions (IIDX 9, 11,
12) use different checksum algorithms for the full payload; IIDX 10 and later
use a different CRC16 variant. **[Verified — card-mag.c `ezusb_iidx_emu_card_mag_update_checksums`]**

Card ID length is 8 bytes. Two card data layouts exist: the 9th-style layout
(`data_sector_9th`) and the standard layout (`data_sector`). **[Verified — card-mag.c]**

---

## 10. Other Nodes

### 10.1 SECURITY_PLUG (node 0x01)

Handles the Konami security dongle interface. Two slots: BLACK (0x00) and WHITE
(0x01). Two memory types: ROM (0x00) and DATA (0x01).

V1 commands: READ_ROM (0x01), READ_DATA (0x02), WRITE_DATA (0x03),
SELECT_DONGLE_1 (0x04), SELECT_DONGLE_2 (0x05). **[Verified — secplug-cmd.h]**

V2 commands: SEARCH (0x01), READ_DATA (0x02), WRITE_DATA (0x03), READ_ROM
(0x06), SELECT_DONGLE_1–5 (0x07–0x0B). Status codes are distinct per operation
(0x12–0x16). **[Verified — secplug-cmd.h]**

### 10.2 EEPROM (node 0x02)

Persistent storage. Same command codes for V1 and V2: READ (0x02), WRITE (0x03).
Status codes differ by version.

V1: READ_OK=0x01, WRITE_OK=0x02, FAULT=0xFE.
V2: READ_OK=0x21, WRITE_OK=0x22, FAULT=0xFE. **[Verified — eeprom-cmd.h]**

### 10.3 COIN (node 0x09)

Controls the coin counter and tracks coin acceptance mode.

Commands: SET_COIN_MODE_1 (0x01) — used when "playing", SET_COIN_MODE_2 (0x02)
— used in service menu. **[Verified — coin-cmd.h]**

The current coin mode affects the `inverted_pad` bit 31 (coin mode state) in the
interrupt read packet. Mode 1 → bit 31 = 1 (after inversion = 0). **[Verified — msg.c]**

### 10.4 WDT (node 0x0C)

Watchdog timer. Single command: INIT (0x3C), status OK=0x00, FAULT=0xFE.
**[Verified — wdt-cmd.h]**

### 10.5 SRAM (node 0x40)

Static RAM storage. Accessed via bulk read/write packets. No specific command
constants defined beyond the node interface. **[Corroborated — node appears in
both V1 and V2 tables]**

### 10.6 16SEG (node 0x05)

16-segment display (marquee). Present in both V1 and V2 tables. Driven by
`iidx_io_ep3_write_16seg()` through the iidxio API. **[Corroborated — node
appears in both node tables; 16seg emu module exists]**

### 10.7 SECURITY_MEM (node 0xFE)

Security memory node. Separate from the security plug. Present in both V1 and
V2 tables with different implementations. **[Verified — msg.c node tables]**

---

## 11. Board Type Differences (C02 vs D01)

Two board variants are emulated: **[Verified — `ezusb_iidx_emu_msg_io_board_type` enum]**

| Constant | Value | Board |
|----------|-------|-------|
| `EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02` | 0 | C02 board |
| `EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_D01` | 1 | D01 board |

The sole behavioral difference in the interrupt read packet is bit 4 of
`inverted_pad`: **[Verified — msg.c board type switch]**

- **C02**: bit 4 is unmodified (set by hardware, reflects dip switch).
- **D01**: bit 4 is forced to 0 after inversion (`msg_resp->inverted_pad &= ~(1 << 4)`).

According to the msg.h comment: "C02/D01 board identifier, 1 = C02, 0 = D01
(on active low). This defines how the game has to flash the FPGA board since D01
needs a different firmware." **[Verified — msg.h comment on inverted_pad bit 4]**

D01 boards require different FPGA firmware. The bit 4 value is how the game
detects which firmware to load at boot. **[Corroborated — msg.h comment]**

The V1 init function takes an `io_board_type` parameter. The V2 init function
(`ezusb_iidx_emu_msg_v2_init`) does not, as V2 was only used with C02 boards.
**[Verified — msg.h function signatures]**
