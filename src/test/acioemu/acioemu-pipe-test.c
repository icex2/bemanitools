/*
 * ACIO pipe framing unit tests.
 *
 * Tests derived from doc/protocol/acio.md (the ACIO protocol reference).
 * Each test verifies implementation behavior against documented wire format
 * specifications, not against emulator source code directly.
 *
 * Coverage:
 *   - Round-trip command message parsing
 *   - SOF byte (0xAA) escape encoding
 *   - ESCAPE byte (0xFF) escape encoding
 *   - Checksum validation (bad checksum rejected)
 *   - Autobaud frame ([SOF, SOF] yields NULL message)
 *   - Broadcast message parsing (addr=0x70)
 *   - Truncated frame recovery (SOF mid-frame resets parser)
 *   - Response serialization (drain produces correctly escaped/checksummed bytes)
 */

#include <stdint.h>
#include <string.h>

#include "acio/acio.h"
#include "acioemu/pipe.h"
#include "test/check.h"
#include "test/test.h"
#include "util/iobuf.h"

/*
 * test_round_trip_command: Supply a well-formed GET_VERSION wire frame and
 * verify ac_io_out_get_message returns the decoded fields.
 *
 * Wire frame (from doc/protocol/acio.md Section 4 example):
 *   AA  01  00 02  00  00  03
 *   SOF addr code  seq nbytes checksum
 *   checksum = 0x01+0x00+0x02+0x00+0x00 = 0x03
 */
static void test_round_trip_command(void)
{
    struct ac_io_out out;
    uint8_t wire[] = {
        AC_IO_SOF,
        0x01,        /* addr=1 */
        0x00, 0x02,  /* code=GET_VERSION big-endian */
        0x00,        /* seq_no=0 */
        0x00,        /* nbytes=0 */
        0x03,        /* checksum: 0x01+0x00+0x02+0x00+0x00=0x03 */
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};
    const struct ac_io_message *msg;

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_true(ac_io_out_have_message(&out));

    msg = ac_io_out_get_message(&out);
    check_non_null(msg);
    check_int_eq(msg->addr, 0x01);
    check_int_eq(ac_io_u16(msg->cmd.code), AC_IO_CMD_GET_VERSION);
    check_int_eq(msg->cmd.seq_no, 0x00);
    check_int_eq(msg->cmd.nbytes, 0x00);
}

/*
 * test_round_trip_with_payload: Supply a command with a 2-byte payload and
 * verify all fields including payload bytes are decoded correctly.
 *
 * Wire frame:
 *   AA  02  00 80  05  02  AA BB  XX
 *   Wait — 0xAA in payload must be escaped. Construct unescaped logic manually:
 *
 *   addr=2, code=KEEPALIVE(0x0080), seq_no=5, nbytes=2, payload=[0x01,0x02]
 *   checksum = 0x02+0x00+0x80+0x05+0x02+0x01+0x02 = 0x8C
 */
static void test_round_trip_with_payload(void)
{
    struct ac_io_out out;
    uint8_t wire[] = {
        AC_IO_SOF,
        0x02,        /* addr=2 */
        0x00, 0x80,  /* code=KEEPALIVE */
        0x05,        /* seq_no=5 */
        0x02,        /* nbytes=2 */
        0x01, 0x02,  /* payload */
        0x8C,        /* checksum: 0x02+0x00+0x80+0x05+0x02+0x01+0x02=0x8C */
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};
    const struct ac_io_message *msg;

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_true(ac_io_out_have_message(&out));

    msg = ac_io_out_get_message(&out);
    check_non_null(msg);
    check_int_eq(msg->addr, 0x02);
    check_int_eq(ac_io_u16(msg->cmd.code), AC_IO_CMD_KEEPALIVE);
    check_int_eq(msg->cmd.seq_no, 0x05);
    check_int_eq(msg->cmd.nbytes, 0x02);
    check_int_eq(msg->cmd.raw[0], 0x01);
    check_int_eq(msg->cmd.raw[1], 0x02);
}

/*
 * test_escape_sof_in_payload: Per doc/protocol/acio.md Section 3, a payload
 * byte of 0xAA must be escaped as [0xFF, 0x55] on the wire. The parser must
 * recover the original byte.
 *
 * addr=1, code=0x0003(START_UP), seq_no=0, nbytes=1, payload=[0xAA]
 * checksum = 0x01+0x00+0x03+0x00+0x01+0xAA = 0xAF
 * 0xAA in payload → [0xFF, 0x55]; 0xAF in checksum → literal (no escape needed)
 */
static void test_escape_sof_in_payload(void)
{
    struct ac_io_out out;
    /* Wire bytes: SOF unescaped; payload 0xAA escaped as [0xFF, 0x55] */
    uint8_t wire[] = {
        AC_IO_SOF,
        0x01,              /* addr */
        0x00, 0x03,        /* code=START_UP */
        0x00,              /* seq_no */
        0x01,              /* nbytes=1 */
        0xFF, 0x55,        /* payload 0xAA escaped as [0xFF, ~0xAA=0x55] */
        0xAF,              /* checksum: 0x01+0x00+0x03+0x00+0x01+0xAA=0xAF */
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};
    const struct ac_io_message *msg;

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_true(ac_io_out_have_message(&out));

    msg = ac_io_out_get_message(&out);
    check_non_null(msg);
    check_int_eq(msg->cmd.nbytes, 0x01);
    check_int_eq(msg->cmd.raw[0], 0xAA); /* recovered after unescaping */
}

/*
 * test_escape_escape_byte_in_payload: Per doc/protocol/acio.md Section 3, a
 * payload byte of 0xFF must be escaped as [0xFF, 0x00] on the wire.
 *
 * addr=1, code=0x0003, seq_no=0, nbytes=1, payload=[0xFF]
 * checksum = 0x01+0x00+0x03+0x00+0x01+0xFF = 0x04 (wraps at 8 bits)
 * 0xFF in payload → [0xFF, 0x00]; checksum 0x04 → literal
 */
static void test_escape_escape_byte_in_payload(void)
{
    struct ac_io_out out;
    uint8_t wire[] = {
        AC_IO_SOF,
        0x01,
        0x00, 0x03,
        0x00,
        0x01,
        0xFF, 0x00, /* payload 0xFF escaped as [0xFF, ~0xFF=0x00] */
        0x04,       /* checksum: 0x01+0x00+0x03+0x00+0x01+0xFF=0x04 */
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};
    const struct ac_io_message *msg;

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_true(ac_io_out_have_message(&out));

    msg = ac_io_out_get_message(&out);
    check_non_null(msg);
    check_int_eq(msg->cmd.nbytes, 0x01);
    check_int_eq(msg->cmd.raw[0], 0xFF); /* recovered after unescaping */
}

/*
 * test_bad_checksum_rejected: Per doc/protocol/acio.md Section 4, a message
 * with an incorrect checksum must be silently dropped. have_message stays
 * false after supply.
 *
 * Use the GET_VERSION frame from test_round_trip_command but corrupt the
 * checksum byte.
 */
static void test_bad_checksum_rejected(void)
{
    struct ac_io_out out;
    uint8_t wire[] = {
        AC_IO_SOF,
        0x01,
        0x00, 0x02,
        0x00,
        0x00,
        0xDE, /* wrong checksum (correct is 0x03) */
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_false(ac_io_out_have_message(&out));
}

/*
 * test_autobaud_frame: Per doc/protocol/acio.md Section 5, two consecutive SOF
 * bytes ([0xAA, 0xAA]) produce a NULL message pointer from
 * ac_io_out_get_message(). have_message is true but get_message returns NULL.
 */
static void test_autobaud_frame(void)
{
    struct ac_io_out out;
    uint8_t wire[] = {AC_IO_SOF, AC_IO_SOF};
    struct const_iobuf src = {wire, sizeof(wire), 0};

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_true(ac_io_out_have_message(&out));
    check_null(ac_io_out_get_message(&out));
}

/*
 * test_broadcast_message: Per doc/protocol/acio.md Section 6, address 0x70
 * uses the broadcast struct layout (nbytes + raw, no code/seq_no).
 *
 * Wire: SOF  0x70  0x02  0xAB 0xCD  checksum
 * checksum = 0x70+0x02+0xAB+0xCD = 0x54 (mod 256: 0x70+0x02=0x72, +0xAB=0x1D, +0xCD=0xEA)
 * Wait: 0x70+0x02+0xAB+0xCD = 0x70+0x02+0xAB+0xCD = 0x14E -> 0x4E
 */
static void test_broadcast_message(void)
{
    struct ac_io_out out;
    /*
     * addr=0x70, nbytes=2, payload=[0xAB,0xCD]
     * checksum = 0x70+0x02+0xAB+0xCD = 0x4E (truncated 8-bit)
     * 0x70+0x02 = 0x72; +0xAB = 0x1D (wrapped); 0x1D+0xCD = 0xEA
     * Re-calc: 0x70=112, 0x02=2, 0xAB=171, 0xCD=205 -> 112+2+171+205=490 -> 490%256=234=0xEA
     */
    uint8_t wire[] = {
        AC_IO_SOF,
        AC_IO_BROADCAST, /* 0x70 */
        0x02,            /* nbytes=2 */
        0xAB, 0xCD,      /* payload */
        0xEA,            /* checksum: (0x70+0x02+0xAB+0xCD) & 0xFF = 0xEA */
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};
    const struct ac_io_message *msg;

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_true(ac_io_out_have_message(&out));

    msg = ac_io_out_get_message(&out);
    check_non_null(msg);
    check_int_eq(msg->addr, AC_IO_BROADCAST);
    check_int_eq(msg->bcast.nbytes, 0x02);
    check_int_eq(msg->bcast.raw[0], 0xAB);
    check_int_eq(msg->bcast.raw[1], 0xCD);
}

/*
 * test_truncated_frame_recovery: Per doc/protocol/acio.md Section 2, a SOF
 * byte appearing mid-frame resets the parser. The incomplete frame is discarded
 * and parsing continues from the new SOF.
 *
 * Send: SOF addr [partial data] SOF [complete second frame]
 * Expect: second frame is parsed correctly.
 */
static void test_truncated_frame_recovery(void)
{
    struct ac_io_out out;
    /*
     * First frame (truncated): SOF addr=1 code=0x0002 seq=0 ... then cut off
     * Second frame (complete): GET_VERSION to addr=2
     *   checksum: 0x02+0x00+0x02+0x00+0x00 = 0x04
     */
    uint8_t wire[] = {
        /* truncated first frame */
        AC_IO_SOF, 0x01, 0x00, 0x02, 0x00, /* incomplete, no nbytes yet */
        /* start of second complete frame */
        AC_IO_SOF,
        0x02,       /* addr=2 */
        0x00, 0x02, /* code=GET_VERSION */
        0x00,       /* seq_no=0 */
        0x00,       /* nbytes=0 */
        0x04,       /* checksum: 0x02+0x00+0x02+0x00+0x00=0x04 */
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};
    const struct ac_io_message *msg;

    ac_io_out_init(&out);
    ac_io_out_supply(&out, &src);

    check_bool_true(ac_io_out_have_message(&out));

    msg = ac_io_out_get_message(&out);
    check_non_null(msg);
    check_int_eq(msg->addr, 0x02); /* second frame was parsed */
    check_int_eq(ac_io_u16(msg->cmd.code), AC_IO_CMD_GET_VERSION);
}

/*
 * test_response_serialization: ac_io_in_supply with a message and delay=0,
 * then ac_io_in_drain into a buffer, must produce SOF + escaped + checksummed
 * bytes matching the documented wire format.
 *
 * Supply: addr=0x81 (addr=1 | RESPONSE_FLAG), code=GET_VERSION, seq_no=0,
 *         nbytes=0. Expected checksum: 0x81+0x00+0x02+0x00+0x00 = 0x83.
 * Wire:   AA 81 00 02 00 00 83
 */
static void test_response_serialization(void)
{
    struct ac_io_in in;
    struct ac_io_message msg;
    uint8_t buf[64];
    struct iobuf dest = {buf, sizeof(buf), 0};

    uint8_t expected[] = {
        AC_IO_SOF,
        0x81,       /* addr=1 | 0x80 = 0x81 */
        0x00, 0x02, /* code=GET_VERSION, big-endian */
        0x00,       /* seq_no=0 */
        0x00,       /* nbytes=0 */
        0x83,       /* checksum: 0x81+0x00+0x02+0x00+0x00=0x83 */
    };

    memset(&msg, 0, sizeof(msg));
    msg.addr = 0x81;
    msg.cmd.code = ac_io_u16(AC_IO_CMD_GET_VERSION);
    msg.cmd.seq_no = 0x00;
    msg.cmd.nbytes = 0x00;

    ac_io_in_init(&in);
    ac_io_in_supply(&in, &msg, 0); /* delay=0: immediate */
    ac_io_in_drain(&in, &dest);

    check_data_eq(buf, dest.pos, expected, sizeof(expected));
}

/*
 * test_response_serialization_escape: Verify that 0xAA in the addr byte of a
 * response is correctly escaped on drain.
 *
 * Node addr 0x2A: response addr = 0x2A | 0x80 = 0xAA -> escaped as [0xFF, 0x55]
 * msg: addr=0xAA, code=KEEPALIVE(0x0080), seq=0, nbytes=0
 * checksum = 0xAA+0x00+0x80+0x00+0x00 = 0x2A
 * Wire: AA [FF 55] 00 80 00 00 2A
 */
static void test_response_serialization_escape(void)
{
    struct ac_io_in in;
    struct ac_io_message msg;
    uint8_t buf[64];
    struct iobuf dest = {buf, sizeof(buf), 0};

    uint8_t expected[] = {
        AC_IO_SOF,
        0xFF, 0x55, /* addr=0xAA escaped */
        0x00, 0x80, /* code=KEEPALIVE */
        0x00,       /* seq_no */
        0x00,       /* nbytes */
        0x2A,       /* checksum: 0xAA+0x00+0x80+0x00+0x00=0x2A */
    };

    memset(&msg, 0, sizeof(msg));
    msg.addr = 0xAA;
    msg.cmd.code = ac_io_u16(AC_IO_CMD_KEEPALIVE);
    msg.cmd.seq_no = 0x00;
    msg.cmd.nbytes = 0x00;

    ac_io_in_init(&in);
    ac_io_in_supply(&in, &msg, 0);
    ac_io_in_drain(&in, &dest);

    check_data_eq(buf, dest.pos, expected, sizeof(expected));
}

TEST_MODULE_BEGIN("acioemu-pipe")
TEST_MODULE_TEST(test_round_trip_command)
TEST_MODULE_TEST(test_round_trip_with_payload)
TEST_MODULE_TEST(test_escape_sof_in_payload)
TEST_MODULE_TEST(test_escape_escape_byte_in_payload)
TEST_MODULE_TEST(test_bad_checksum_rejected)
TEST_MODULE_TEST(test_autobaud_frame)
TEST_MODULE_TEST(test_broadcast_message)
TEST_MODULE_TEST(test_truncated_frame_recovery)
TEST_MODULE_TEST(test_response_serialization)
TEST_MODULE_TEST(test_response_serialization_escape)
TEST_MODULE_END()
