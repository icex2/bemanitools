# ACIO Protocol Reference

Reverse-engineered protocol reference for the ACIO serial bus used in Konami
arcade rhythm game I/O hardware. Derived from `acioemu/pipe.c`, `acioemu/emu.c`,
`acioemu/icca.c`, `acioemu/addr.c`, and `acio/acio.h`. Confidence tags mark the
source of each fact:

- `<!-- [Verified] -->` — code and at least one external source agree
- `<!-- [Corroborated] -->` — code matches at least one external source
- `<!-- [Inferred] -->` — code only, no external confirmation

## 1. Overview

<!-- [Inferred] -->

ACIO (Amusement Controller I/O) is a Konami proprietary serial bus protocol
used to communicate with I/O boards attached to arcade rhythm game cabinets.
Physical transport is RS-232 or USB-to-serial (CDC/ACM class). The game side
runs `libacio.dll`; the device side is a microcontroller on each I/O board.

Bemanitools emulates the device side: the ACIO emulator (`acioemu/`) intercepts
serial port IRP calls from the game and responds as if real hardware were
present.

This document covers:
- Wire frame format and escape encoding
- Checksum calculation
- Autobaud handshake
- Broadcast messages
- Response flag
- Address assignment sequence
- Legacy mode (old libacio compatibility)
- ICCA card reader node state machine
- Standard node commands (GET_VERSION, START_UP, KEEPALIVE)

BIO2 uses the same framing layer over a USB-serial interface. See
`doc/protocol/bio2.md` for BIO2-specific details.

## 2. Wire Frame Format

<!-- [Verified] -->

Every ACIO command message on the wire begins with a Start of Frame byte
(`0xAA`) followed by the message payload and ends with a checksum byte. The
frame format differs for command messages (addr ≠ 0x70) and broadcast messages
(addr = 0x70).

### Command Frame

Used for all unicast commands and responses to/from individual nodes.

| Byte(s)         | Field      | Description                                    |
| --------------- | ---------- | ---------------------------------------------- |
| `0xAA`          | SOF        | Start of Frame, always literal `0xAA`          |
| +0              | `addr`     | Node address (1..N); high bit set on responses |
| +1..+2          | `code`     | Command code, big-endian `uint16_t`            |
| +3              | `seq_no`   | Sequence number, echoed unchanged in response  |
| +4              | `nbytes`   | Payload length in bytes                        |
| +5..(5+nbytes-1) | payload  | Command-specific data (`nbytes` bytes)         |
| +(5+nbytes)     | checksum   | 8-bit sum of all bytes from `addr` through end of payload |

The SOF byte is always literal `0xAA` and is never escaped. Only bytes in the
payload (and the checksum byte itself, if it happens to be `0xAA` or `0xFF`)
are subject to escape encoding. See Section 3.

**Struct layout** (from `src/main/acio/acio.h`, `#pragma pack(1)`):

```c
struct ac_io_message {
    uint8_t addr;   /* bit 7 clear = request, bit 7 set = response */
    struct {
        uint16_t code;    /* big-endian */
        uint8_t  seq_no;
        uint8_t  nbytes;
        uint8_t  raw[0xFF];
    } cmd;
};
```

Total wire byte count (before escape): 1 (SOF) + 1 (addr) + 2 (code) + 1
(seq_no) + 1 (nbytes) + nbytes (payload) + 1 (checksum).

### Broadcast Frame

<!-- [Inferred] -->

Used for bus-wide broadcasts. Address is always `0x70` (`AC_IO_BROADCAST`).
The broadcast struct omits `code` and `seq_no`:

| Byte(s)          | Field     | Description                              |
| ---------------- | --------- | ---------------------------------------- |
| `0xAA`           | SOF       | Start of Frame                           |
| +0               | `addr`    | Always `0x70`                            |
| +1               | `nbytes`  | Payload length in bytes                  |
| +2..(1+nbytes)   | payload   | Broadcast-specific data                  |
| +(2+nbytes)      | checksum  | 8-bit sum from `addr` through end of payload |

**Struct layout**:

```c
struct ac_io_message {
    uint8_t addr;   /* 0x70 */
    struct {
        uint8_t nbytes;
        uint8_t raw[0xFF];
    } bcast;
};
```

The parser in `ac_io_out_supply_frame_byte()` dispatches to
`ac_io_out_detect_broadcast_eof()` or `ac_io_out_detect_command_eof()` based
on the value of `msg.addr` after the address byte is received.

## 3. Escape Encoding

<!-- [Verified] -->

Two byte values are reserved control bytes on the wire:

| Value  | Name            | Meaning        |
| ------ | --------------- | -------------- |
| `0xAA` | `AC_IO_SOF`    | Start of Frame |
| `0xFF` | `AC_IO_ESCAPE` | Escape byte    |

When either control byte value appears as a data byte (in the payload or in the
checksum), it must be escaped before transmission. The escape rule:

```
escape(b) = [0xFF, ~b]
```

| Data byte | Escaped sequence  |
| --------- | ----------------- |
| `0xAA`    | `[0xFF, 0x55]`    |
| `0xFF`    | `[0xFF, 0x00]`    |

Source: `ac_io_in_queued_putc()` in `acioemu/pipe.c`:

```c
if (b == AC_IO_SOF || b == AC_IO_ESCAPE) {
    iq->bytes[iq->iobuf.nbytes++] = AC_IO_ESCAPE;
    iq->bytes[iq->iobuf.nbytes++] = ~b;
} else {
    iq->bytes[iq->iobuf.nbytes++] = b;
}
```

On receive, the parser in `ac_io_out_supply_frame_byte()` reverses this:

```c
if (out->escape) {
    out->escape = false;
    b = ~b;    /* recover original byte */
} else if (b == AC_IO_ESCAPE) {
    out->escape = true;
    return true;
}
```

The SOF byte is only used as a frame delimiter; it is never escaped in the
framing header position. Only payload/checksum bytes are escaped.

## 4. Checksum

<!-- [Verified] -->

The checksum is an 8-bit (modulo 256) sum of all message bytes from `addr`
through the last payload byte, inclusive. The SOF byte is not included.

Serialization in `ac_io_in_queued_populate()`:

```c
checksum = 0;
for (i = 0; i < nbytes; i++) {
    ac_io_in_queued_putc(iq, src[i]);
    checksum += src[i];
}
ac_io_in_queued_putc(iq, checksum);
```

`nbytes` here is computed as:
- Command: `offsetof(struct ac_io_message, cmd.raw) + msg->cmd.nbytes`
- Broadcast: `offsetof(struct ac_io_message, bcast.raw) + msg->bcast.nbytes`

This means the sum covers: `addr`, `code` (2 bytes), `seq_no`, `nbytes` field,
and all payload bytes — but not the SOF and not the checksum byte itself.

Validation on receive in `ac_io_out_check_sum()`:

```c
checksum = 0;
for (i = 0; i < out->pos - 1; i++) {
    checksum += out->bytes[i];
}
/* out->bytes[out->pos - 1] is the received checksum byte */
if (checksum == out->bytes[out->pos - 1]) {
    /* accept */
} else {
    /* reject: reset state, continue scanning */
}
```

A message with an incorrect checksum is silently dropped and parsing continues.
The `have_message` flag is not set.

### Example: GET_VERSION to node 1

```
Wire: AA  01  00 02  00  00  03
      SOF addr code  seq nbytes checksum

Checksum = 0x01 + 0x00 + 0x02 + 0x00 + 0x00 = 0x03
```

## 5. Autobaud Frame

<!-- [Inferred] -->

Some versions of `libacio` send two consecutive SOF bytes (`0xAA 0xAA`) as an
autobaud probe at startup. The parser handles this case explicitly:

In `ac_io_out_supply_frame_byte()`, when a second SOF arrives before any
payload bytes have been accumulated (`out->pos == 0`), the parser sets
`have_message = true` and returns a NULL message pointer:

```c
} else if (b == AC_IO_SOF) {
    if (out->pos == 0) {
        /* Got autobaud/empty message */
        out->have_message = true;
        return false;
    } else {
        log_warning("Truncated message");
        out->pos = 0;
        return true;
    }
}
```

`ac_io_out_get_message()` returns `NULL` when `out->pos == 0`:

```c
const struct ac_io_message *ac_io_out_get_message(const struct ac_io_out *out)
{
    if (out->pos == 0) {
        return NULL;  /* autobaud/empty frame */
    } else {
        return &out->msg;
    }
}
```

The emulator responds to an autobaud probe by queuing a single SOF byte back:

```c
/* from acioemu/emu.c */
ac_io_in_supply(&emu->in, NULL, 0);  /* NULL msg = SOF only */
```

`ac_io_in_supply()` with `msg == NULL` skips `ac_io_in_queued_populate()`,
leaving the queue entry with only the pre-written SOF byte.

## 6. Broadcast Messages

<!-- [Inferred] -->

Broadcast address is `0x70` (`AC_IO_BROADCAST`). A broadcast message uses a
different struct layout with no `code` or `seq_no` fields — only `nbytes` and
raw payload.

The parser detects this after receiving the address byte:

```c
if (out->msg.addr == AC_IO_BROADCAST) {
    return ac_io_out_detect_broadcast_eof(out);
} else {
    return ac_io_out_detect_command_eof(out);
}
```

End-of-frame detection for broadcast:

```c
static bool ac_io_out_detect_broadcast_eof(struct ac_io_out *out)
{
    if (out->pos > offsetof(struct ac_io_message, bcast.nbytes)) {
        end = offsetof(struct ac_io_message, bcast.raw) +
            out->msg.bcast.nbytes + 1;  /* +1 for checksum */
        if (out->pos == end) {
            return ac_io_out_check_sum(out);
        }
    }
    return true;
}
```

Broadcast messages are observed during bus enumeration. The exact usage of
broadcast payloads in game code is not documented here.

## 7. Response Flag

<!-- [Verified] -->

When a node sends a response to a command, it sets bit 7 of the address byte:

```c
resp.addr = req->addr | AC_IO_RESPONSE_FLAG;  /* AC_IO_RESPONSE_FLAG = 0x80 */
```

So if the host sent a command to node address `0x01`, the node responds with
address byte `0x81`. This bit is part of the serialized `addr` field and is
subject to escape encoding if `addr | 0x80` happens to equal `0xAA` or `0xFF`
(e.g., node address `0x2A` → response addr `0xAA` → escaped as `[0xFF, 0x55]`;
node address `0x7F` → response addr `0xFF` → escaped as `[0xFF, 0x00]`).

## 8. Address Assignment

<!-- [Verified] -->

At startup the host enumerates all nodes on the bus via a broadcast-like
assignment sequence.

1. Host sends `AC_IO_CMD_ASSIGN_ADDRS` (`0x0001`) to address `0x00`.
2. Each node on the bus claims the next available address in sequence.
3. The response (from address `0x00`) contains the total node count in
   `cmd.raw[0]` (`cmd.count`).
4. Subsequent commands go to addresses `0x01` through `0x{node_count}`.

Response construction in `acioemu/addr.c`:

```c
resp.addr = 0;                        /* response addr 0 | 0x80 = 0x80 */
resp.cmd.code = req->cmd.code;        /* echoed */
resp.cmd.seq_no = req->cmd.seq_no;    /* echoed */
resp.cmd.nbytes = sizeof(resp.cmd.count);  /* 1 byte */
resp.cmd.count = node_count;
```

Wait — the emulator sets `resp.addr = 0`, not `0 | AC_IO_RESPONSE_FLAG`. The
response byte is therefore `0x00`. This may be a peculiarity of this
implementation or the actual protocol. <!-- [Inferred] -->

### Example: Address Assignment Exchange

```
Host → bus:
  AA  00  00 01  00  00  01
  SOF addr code  seq nbytes checksum (0x00+0x00+0x01+0x00+0x00=0x01)

Bus → host (2 nodes found):
  AA  00  00 01  00  01  02  04
  SOF addr code  seq nbytes count checksum
  (0x00+0x00+0x01+0x00+0x01+0x02=0x04)
```

## 9. Legacy Mode

<!-- [Corroborated] -->

Legacy mode is required for Pop'n Music 15–18 which use slotted card readers
with an older version of `libacio`. Those old versions read only the first
response message from each read buffer and discard any remaining bytes. If
multiple responses are batched into a single read buffer, the game errors with
code `0x00000002`.

Enabling legacy mode:

```c
void ac_io_legacy_mode(void)
{
    log_info("Running acioemu legacy mode");
    ac_io_enable_legacy_mode = true;
}
```

Effect in `ac_io_in_drain()`:

```c
if (ac_io_enable_legacy_mode) {
    break;   /* drain only one queued response per call */
}
```

Without legacy mode, `ac_io_in_drain()` loops until the queue is empty or the
destination buffer is full, batching multiple responses. With legacy mode, it
drains exactly one response per call.

Newer `libacio` versions (e.g., Copula / Pop'n 20+) reject legacy mode — if
legacy mode is enabled for a newer game, the game will error with the same
error code `0x00000002` because it expects to receive all buffered responses in
one read.

Legacy mode is activated by `popnhook_acio_init(true)` via the hook's init path.

## 10. ICCA Node State Machine

<!-- [Inferred] -->

ICCA (IC Card Authenticator) is the Konami e-amusement card reader node. It
handles magnetic stripe cards (ISO 15693), FeliCa contactless cards, and a
numeric keypad. One ICCA node may represent one card reader unit; a cabinet
often has two (player 1 and player 2, at unit indices 0 and 1).

### Version Variants

The emulator supports three hardware versions, configured via
`ac_io_emu_icca_set_version()`:

| Enum   | Minor version | Notes                                         |
| ------ | ------------- | --------------------------------------------- |
| `v150` | 1.5.0         | Older slotted reader (popn 15–18 era)         |
| `v160` | 1.6.0         | Default; most common (IIDX, SDVX, jubeat era) |
| `v170` | 1.7.0         | Wave-pass wavepass reader with FeliCa support |

Version is reported in the `GET_VERSION` response (`cmd.version.minor`).

### Command Set

All commands are unicast to the ICCA node's assigned address. Byte order for
`cmd.code` is big-endian on the wire (`ac_io_u16()` swaps for host).

#### Standard Commands (shared across node types)

| Code     | Name                         | Response                        |
| -------- | ---------------------------- | ------------------------------- |
| `0x0001` | `AC_IO_CMD_ASSIGN_ADDRS`    | See Section 8                   |
| `0x0002` | `AC_IO_CMD_GET_VERSION`     | `struct ac_io_version` (36 bytes) |
| `0x0003` | `AC_IO_CMD_START_UP`        | Status byte `0x00`              |
| `0x0080` | `AC_IO_CMD_KEEPALIVE`       | Empty response (`nbytes=0`)     |
| `0x0100` | `AC_IO_CMD_CLEAR`           | Status byte `0x00`              |

#### ICCA-Specific Commands

| Code     | Name                                 | Response                                    |
| -------- | ------------------------------------ | ------------------------------------------- |
| `0x0130` | `AC_IO_ICCA_CMD_QUEUE_LOOP_START`   | Status `0x00`; clears fault; starts polling |
| `0x0131` | `AC_IO_ICCA_CMD_ENGAGE`             | `struct ac_io_icca_state`                   |
| `0x0134` | `AC_IO_ICCA_CMD_POLL`               | `struct ac_io_icca_state`                   |
| `0x0135` | `AC_IO_ICCA_CMD_SET_SLOT_STATE`     | Status byte (subcmd echo); slot control     |
| `0x013A` | `AC_IO_ICCA_CMD_DEVICE_CONTROL`     | Status `0x00`; enables keypad reports       |
| `0x0160` | `AC_IO_ICCA_CMD_KEY_EXCHANGE`       | 4-byte reader key                           |
| `0x0161` | `AC_IO_ICCA_CMD_POLL_FELICA`        | Version-dependent (see below)               |
| `0x0164` | `AC_IO_ICCA_CMD_POLL_ENCRYPTED`     | Encrypted `struct ac_io_icca_state` + CRC16 |

Unknown commands seen in the wild (handled with status `0x00`):

| Code     | Game context                                |
| -------- | ------------------------------------------- |
| `0x00FF` | First seen on jubeat (1)                    |
| `0x0116` | jubeat — sent after QUEUE_LOOP_START        |
| `0x0120` | jubeat (1) — unknown purpose                |

### State Lifecycle

<!-- [Inferred] -->

The required initialization sequence before polling works:

1. `GET_VERSION` — identify the node type and firmware version
2. `START_UP` — transitions node out of reset; clears `detected_new_reader`
3. `QUEUE_LOOP_START` — clears fault flag; sets `polling_started = true`
4. `POLL` (or `POLL_ENCRYPTED`) — begins steady-state polling loop

Until `QUEUE_LOOP_START` is received, all `POLL` responses return
`status_code = AC_IO_ICCA_STATUS_FAULT` regardless of actual sensor state.
This guard prevents the game from acting on stale sensor data during startup.

For wavepass readers (v160+), the game also sends:

5. `KEY_EXCHANGE` — sets up encryption keys for `POLL_ENCRYPTED`
6. `DEVICE_CONTROL` — enables keypad mode; sets `keypad_started = true`

### GET_VERSION Response

<!-- [Inferred] -->

Response payload is `struct ac_io_version`:

```c
struct ac_io_version {
    uint32_t type;          /* node type, big-endian: 0x03000000 for ICCA */
    uint8_t  flag;          /* 0x00 */
    uint8_t  major;         /* 0x01 */
    uint8_t  minor;         /* 0x05/0x06/0x07 depending on version */
    uint8_t  revision;      /* 0x00 */
    char     product_code[4]; /* "ICCA", "ICCB", or "ICCC" */
    char     date[16];      /* build date string */
    char     time[16];      /* build time string */
};
```

Node type `AC_IO_NODE_TYPE_ICCA = 0x03000000`.

### POLL Response: struct ac_io_icca_state

<!-- [Inferred] -->

Response payload for `POLL`, `POLL_ENCRYPTED`, and `ENGAGE`:

```c
struct ac_io_icca_state {
    uint8_t  status_code;    /* see status codes below */
    uint8_t  sensor_state;   /* bit flags: 0x10=front, 0x20=rear, 0x40=solenoid */
    uint8_t  uid[8];         /* card UID (e-amusement ID bytes) */
    uint8_t  card_type;      /* 0=ISO15693, 1=FeliCa (wavepass readers) */
    uint8_t  keypad_started; /* 0x03 when keypad active, else 0x00 */
    uint8_t  key_events[2];  /* encoded recent keypad events */
    uint16_t key_state;      /* current keypad bitmask, big-endian */
};
```

Status codes:

| Value  | Name                            | Meaning                                |
| ------ | ------------------------------- | -------------------------------------- |
| `0x00` | `AC_IO_ICCA_STATUS_FAULT`      | Error / not ready (default before QUEUE_LOOP_START) |
| `0x01` | `AC_IO_ICCA_STATUS_IDLE`       | Ready, no card present (slotted readers) |
| `0x02` | `AC_IO_ICCA_STATUS_GOT_UID`    | Card read successfully, UID valid      |
| `0x04` | `AC_IO_ICCA_STATUS_IDLE_NEW`   | Ready, no card present (wavepass readers) |

Sensor state bit flags:

| Bit   | Mask   | Meaning                            |
| ----- | ------ | ---------------------------------- |
| bit 4 | `0x10` | Front slot sensor triggered        |
| bit 5 | `0x20` | Rear slot sensor triggered         |
| bit 6 | `0x40` | Solenoid engaged (card held in)    |

`keypad_started` must be `0x03` for slotted reader games (SDVX 4, etc.) to
avoid a "thank you screen hang". It is set when `DEVICE_CONTROL` is received
or `detected_new_reader` is true.

### SET_SLOT_STATE Subcommands

<!-- [Inferred] -->

Request payload is `struct ac_io_icca_misc`:

```c
struct ac_io_icca_misc {
    uint8_t unknown;
    uint8_t subcmd;   /* see slot command codes below */
};
```

Subcmd values and mapping to eamio card slot commands:

| Subcmd | Name                                      | eamio action             |
| ------ | ----------------------------------------- | ------------------------ |
| `0x00` | `AC_IO_ICCA_SUBCMD_CARD_SLOT_CLOSE`      | `EAM_IO_CARD_SLOT_CMD_CLOSE` |
| `0x11` | `AC_IO_ICCA_SUBCMD_CARD_SLOT_OPEN`       | `EAM_IO_CARD_SLOT_CMD_OPEN`  |
| `0x12` | `AC_IO_ICCA_SUBCMD_CARD_SLOT_EJECT`      | `EAM_IO_CARD_SLOT_CMD_EJECT`; clears `engaged` |
| `0x03` | (unnamed)                                 | `EAM_IO_CARD_SLOT_CMD_READ`  |

For v150, `SET_SLOT_STATE` behaves like `POLL` (returns state directly).

### POLL_FELICA Version Branches

<!-- [Inferred] -->

`AC_IO_ICCA_CMD_POLL_FELICA` behavior depends on configured version:

| Version | Response                           | Side effect                       |
| ------- | ---------------------------------- | --------------------------------- |
| v150    | `struct ac_io_icca_state` (like POLL) | none                           |
| v160    | Status `0x01`                      | `detected_new_reader = true`      |
| v170    | Empty response (`nbytes=0`)        | `detected_new_reader = true`      |

`detected_new_reader` changes poll status codes and sensor_state/card_type
interpretation for wavepass readers.

### Encrypted Poll: KEY_EXCHANGE and POLL_ENCRYPTED

<!-- [Inferred] -->

#### KEY_EXCHANGE (0x0160)

Request payload: 4-byte host key (big-endian `uint32_t`).

Response payload: 4-byte reader key. The emulator always responds with the
fixed key `0x14243444` (should be randomized in production hardware, but is
hardcoded here).

Key derivation uses Marsaglia's KISS PRNG with custom initial state:

```c
cipher_keys[0] = reader_key ^ 0x05491333;
cipher_keys[1] = host_key   ^ 0x1F123BB5;
cipher_keys[2] = reader_key ^ 0x159A55E5;
cipher_keys[3] = host_key   ^ 0x075BCD15;
```

#### POLL_ENCRYPTED (0x0164)

Response body: `struct ac_io_icca_state` (16 bytes) + 2-byte CRC16 = 18 bytes,
all encrypted.

Encryption procedure:

1. Compute CRC16-MSB over the 16-byte state: `crc16_msb(resp.cmd.raw, 16, 0)`
2. Append CRC as 2 bytes big-endian at offsets 16–17.
3. XOR-encrypt all 18 bytes using KISS PRNG output:

```c
for (i = 0; i < length; i++) {
    if (i % 4 == 0) {
        /* advance PRNG */
        uint32_t key4_old = keys[3];
        uint32_t key_new  = (key4_old << 11) ^ key4_old;
        uint32_t key1_old = keys[0];
        keys[3] = keys[2];
        keys[2] = keys[1];
        keys[1] = keys[0];
        keys[0] = ((((key1_old >> 11) ^ key_new) >> 8) ^ key_new ^ key1_old);
    }
    data[i] ^= (uint8_t)(keys[0] >> (((3 - (i % 4)) << 3)));
}
```

The PRNG advances once per 4 bytes of output. Keystream byte `i` is extracted
from `keys[0]` using the byte-lane offset `(3 - (i % 4)) * 8`.

### Poll Delay Emulation

<!-- [Inferred] -->

Real hardware imposes a small response latency. The emulator replicates this
by computing the elapsed time since the last poll and using that as the
response delay, capped at 16 ms:

```c
delay_us = time_get_elapsed_us(
    time_get_counter() - icca->time_counter_last_poll);
if (delay_us > 16000) {
    delay_us = 0;
}
```

Messages with `delay_us = 0` drain immediately. Messages with `delay_us > 0`
are held in the queue until the elapsed time threshold is met.

## 11. Standard Node Commands

<!-- [Inferred] -->

These commands are handled uniformly across all ACIO node types.

### GET_VERSION (0x0002)

Request: zero payload (`nbytes = 0`).

Response: `struct ac_io_version` (36 bytes):

```c
struct ac_io_version {
    uint32_t type;          /* node-type constant, big-endian */
    uint8_t  flag;
    uint8_t  major;
    uint8_t  minor;
    uint8_t  revision;
    char     product_code[4];
    char     date[16];
    char     time[16];
};
```

The `type` field identifies the node class. Known values:

| Constant                       | Value        | Node               |
| ------------------------------ | ------------ | ------------------ |
| `AC_IO_NODE_TYPE_ICCA`        | `0x03000000` | IC Card reader     |
| `AC_IO_NODE_TYPE_ICCB`        | `0x03000000` | Same as ICCA       |
| `AC_IO_NODE_TYPE_H44B`        | `0x04010000` | H44B lighting      |
| `AC_IO_NODE_TYPE_LED_STRIP`   | `0x04020000` | LED strip          |
| `AC_IO_NODE_TYPE_LED_SPIKE`   | `0x05010000` | LED spike          |
| `AC_IO_NODE_TYPE_KFCA`        | `0x09060000` | KFCA I/O           |
| `AC_IO_NODE_TYPE_RVOL`        | `0x09060001` | RVOL               |
| `AC_IO_NODE_TYPE_MDXF`        | `0x09070000` | MDXF               |
| `AC_IO_NODE_TYPE_PANB`        | `0x090E0000` | PANB               |
| `AC_IO_NODE_TYPE_BMPU`        | `0x0B000000` | BMPU               |
| `AC_IO_NODE_TYPE_BI2A`        | `0x0D060000` | BIO2 IIDX node     |
| `AC_IO_NODE_TYPE_BIOB`        | `0x0D050100` | BIO2 B             |
| `AC_IO_NODE_TYPE_BI2B`        | `0x0D060100` | BIO2 B2            |

### START_UP (0x0003)

Request: zero payload.

Response: 1-byte status `0x00` on success. For ICCA, also clears
`detected_new_reader`.

### KEEPALIVE (0x0080)

Request: zero payload.

Response: empty (`nbytes = 0`). Used as a heartbeat to confirm the node is
still responsive.

## Byte-Order Note

<!-- [Verified] -->

Multi-byte fields in the `ac_io_message` struct are stored in big-endian order
on the wire. The host (Windows x86) is little-endian. The macros
`ac_io_u16(x)` and `ac_io_u32(x)` perform the byte swap:

```c
#define ac_io_u16(x) _byteswap_ushort(x)
#define ac_io_u32(x) _byteswap_ulong(x)
```

Always apply these when reading `cmd.code` from a received message or when
writing a code into a response struct.
