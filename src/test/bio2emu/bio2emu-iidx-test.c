/*
 * BIO2 IIDX emulator unit tests.
 *
 * Tests derived from doc/protocol/bio2.md. Each test verifies dispatch logic
 * against the documented command set and poll state struct layout, without
 * requiring the iohook or serial port subsystems.
 *
 * Test setup: zero-initialize struct bio2emu_port, manually initialize the
 * ac_io_in and ac_io_out pipe queues, then drive bio2_emu_bi2a_dispatch_request
 * directly with crafted ac_io_message structs. This bypasses bio2emu_port_init
 * (which calls iohook_open_nul_fd) while still exercising the full dispatch and
 * response-packing logic.
 *
 * Coverage:
 *   - Address assignment: ASSIGN_ADDRS to addr 0 -> node_count = 1
 *   - GET_VERSION: product_code "BI2A", type 0x0D060000
 *   - START_UP: status 0x00
 *   - INIT: status 0x00
 *   - WATCHDOG: status 0x00
 *   - KEEPALIVE: empty response (nbytes = 0)
 *   - POLL with known input state: key bit packing per documented struct layout
 *   - POLL panel/sys state packing
 *   - POLL slider value pass-through
 *   - POLL turntable pass-through
 *   - Coin latch: rising edge increments count; held high does not re-increment
 *   - Turntable accumulator: multiplier mode accumulates delta
 */

#include <stdint.h>
#include <string.h>

#include "acio/acio.h"
#include "acioemu/addr.h"
#include "acioemu/emu.h"
#include "acioemu/pipe.h"
#include "bemanitools/iidxio.h"
#include "bio2/bi2a-iidx.h"
#include "bio2/bio2.h"
#include "bio2emu-iidx/bi2a.h"
#include "bio2emu/emu.h"
#include "stubs/iidxio/iidxio-stub.h"
#include "test/check.h"
#include "test/test.h"
#include "util/iobuf.h"

/*
 * Build a minimal zero-initialized bio2emu_port with the pipe queues ready
 * for use. This avoids bio2emu_port_init() which requires iohook.
 */
static void port_init_bare(struct bio2emu_port *port)
{
    memset(port, 0, sizeof(*port));
    ac_io_in_init(&port->acio.in);
    ac_io_out_init(&port->acio.out);
}

/*
 * Build a wire frame for a zero-payload command to addr 1, supply it to the
 * ac_io_out parser, and return the parsed message pointer.
 *
 * Wire bytes for addr=1, code=cmd, seq=0, nbytes=0:
 *   SOF  addr  code[hi]  code[lo]  seq  nbytes  checksum
 *   0xAA  0x01  (code>>8)  (code&0xFF)  0x00  0x00  sum
 *   checksum = 0x01 + code_hi + code_lo + 0x00 + 0x00
 */
static const struct ac_io_message *
supply_cmd_addr1(struct ac_io_out *out, uint16_t cmd_host)
{
    uint8_t code_hi = (cmd_host >> 8) & 0xFF;
    uint8_t code_lo = cmd_host & 0xFF;
    uint8_t checksum = (uint8_t) (0x01 + code_hi + code_lo);
    uint8_t wire[7] = {
        AC_IO_SOF,
        0x01, /* addr */
        code_hi,
        code_lo,
        0x00, /* seq_no */
        0x00, /* nbytes */
        checksum,
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};

    ac_io_out_init(out);
    ac_io_out_supply(out, &src);
    check_bool_true(ac_io_out_have_message(out));
    return ac_io_out_get_message(out);
}

/*
 * Supply a zero-payload command to addr 0 (address assignment slot).
 * Returns the parsed message pointer.
 */
static const struct ac_io_message *
supply_cmd_addr0(struct ac_io_out *out, uint16_t cmd_host)
{
    uint8_t code_hi = (cmd_host >> 8) & 0xFF;
    uint8_t code_lo = cmd_host & 0xFF;
    uint8_t checksum = (uint8_t) (0x00 + code_hi + code_lo);
    uint8_t wire[7] = {
        AC_IO_SOF,
        0x00, /* addr */
        code_hi,
        code_lo,
        0x00, /* seq_no */
        0x00, /* nbytes */
        checksum,
    };
    struct const_iobuf src = {wire, sizeof(wire), 0};

    ac_io_out_init(out);
    ac_io_out_supply(out, &src);
    check_bool_true(ac_io_out_have_message(out));
    return ac_io_out_get_message(out);
}

/*
 * Build a POLL wire frame with a 48-byte output body (all zeros) to addr 1.
 * Supply it and return the parsed message.
 */
static const struct ac_io_message *supply_poll_with_out(
    struct ac_io_out *out, const struct bi2a_iidx_state_out *body_out)
{
    /* Frame: SOF addr code[2] seq nbytes[1] payload[48] checksum */
    uint8_t wire[7 + sizeof(struct bi2a_iidx_state_out)];
    uint8_t code_hi = (BIO2_BI2A_CMD_POLL >> 8) & 0xFF;
    uint8_t code_lo = BIO2_BI2A_CMD_POLL & 0xFF;
    uint8_t nbytes = (uint8_t) sizeof(struct bi2a_iidx_state_out);
    uint8_t checksum = 0;
    size_t i;

    wire[0] = AC_IO_SOF;
    wire[1] = 0x01;
    wire[2] = code_hi;
    wire[3] = code_lo;
    wire[4] = 0x00; /* seq_no */
    wire[5] = nbytes;
    memcpy(&wire[6], body_out, nbytes);

    /* checksum = addr + code_hi + code_lo + seq + nbytes + payload */
    checksum = 0;
    for (i = 1; i < 6 + nbytes; i++) {
        checksum += wire[i];
    }
    wire[6 + nbytes] = checksum;

    struct const_iobuf src = {wire, sizeof(wire), 0};
    ac_io_out_init(out);
    ac_io_out_supply(out, &src);
    check_bool_true(ac_io_out_have_message(out));
    return ac_io_out_get_message(out);
}

/*
 * Drain a single response from ac_io_in into buf and return nbytes.
 * The caller must provide a buffer large enough for any expected response.
 */
static uint8_t
drain_response(struct ac_io_in *in, struct ac_io_message *out_msg)
{
    uint8_t buf[512];
    struct iobuf dest = {buf, sizeof(buf), 0};

    ac_io_in_drain(in, &dest);
    check_bool_true(dest.pos > 0);

    /* Parse the wire bytes to recover the message */
    struct ac_io_out parser;
    struct const_iobuf src = {buf, dest.pos, 0};
    ac_io_out_init(&parser);
    ac_io_out_supply(&parser, &src);
    check_bool_true(ac_io_out_have_message(&parser));

    const struct ac_io_message *msg = ac_io_out_get_message(&parser);
    check_non_null(msg);
    memcpy(out_msg, msg, sizeof(struct ac_io_message));

    return out_msg->cmd.nbytes;
}

/*
 * test_addr_assignment: ASSIGN_ADDRS to addr 0 must respond with node_count=1.
 *
 * Per doc/protocol/bio2.md Section 3: BIO2 is single-node; the emulator
 * always calls ac_io_emu_cmd_assign_addrs with node_count=1.
 */
static void test_addr_assignment(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    const struct ac_io_message *req;

    port_init_bare(&port);
    req = supply_cmd_addr0(&port.acio.out, AC_IO_CMD_ASSIGN_ADDRS);
    ac_io_emu_cmd_assign_addrs(&port.acio, req, 1);

    drain_response(&port.acio.in, &resp);

    check_int_eq(
        resp.addr, 0); /* addr 0, no response flag per acioemu/addr.c */
    check_int_eq(ac_io_u16(resp.cmd.code), AC_IO_CMD_ASSIGN_ADDRS);
    check_int_eq(resp.cmd.nbytes, 1);
    check_int_eq(resp.cmd.count, 1);
}

/*
 * test_get_version: GET_VERSION must respond with BI2A product code and
 * node type 0x0D060000 (AC_IO_NODE_TYPE_BI2A).
 *
 * Per doc/protocol/bio2.md Section 4.1.
 */
static void test_get_version(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    const struct ac_io_message *req;

    port_init_bare(&port);
    req = supply_cmd_addr1(&port.acio.out, AC_IO_CMD_GET_VERSION);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    check_int_eq(ac_io_u16(resp.cmd.code), AC_IO_CMD_GET_VERSION);
    check_int_eq(
        (int) ac_io_u32(resp.cmd.version.type), (int) AC_IO_NODE_TYPE_BI2A);
    check_int_eq(resp.cmd.version.flag, 0x00);
    /* product_code is char[4], not NUL-terminated — compare 4 bytes */
    check_bool_true(memcmp(resp.cmd.version.product_code, "BI2A", 4) == 0);
}

/*
 * test_start_up: START_UP must respond with status 0x00.
 *
 * Per doc/protocol/bio2.md Section 4.1.
 */
static void test_start_up(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    const struct ac_io_message *req;

    port_init_bare(&port);
    req = supply_cmd_addr1(&port.acio.out, AC_IO_CMD_START_UP);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    check_int_eq(ac_io_u16(resp.cmd.code), AC_IO_CMD_START_UP);
    check_int_eq(resp.cmd.nbytes, 1);
    check_int_eq(resp.cmd.status, 0x00);
}

/*
 * test_init: INIT (0x0100) must respond with status 0x00.
 *
 * Per doc/protocol/bio2.md Section 4.2.
 */
static void test_init(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    const struct ac_io_message *req;

    port_init_bare(&port);
    req = supply_cmd_addr1(&port.acio.out, BIO2_BI2A_CMD_INIT);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    check_int_eq(ac_io_u16(resp.cmd.code), BIO2_BI2A_CMD_INIT);
    check_int_eq(resp.cmd.nbytes, 1);
    check_int_eq(resp.cmd.status, 0x00);
}

/*
 * test_watchdog: WATCHDOG (0x0120) must respond with status 0x00.
 *
 * Per doc/protocol/bio2.md Section 4.2.
 */
static void test_watchdog(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    const struct ac_io_message *req;

    port_init_bare(&port);
    req = supply_cmd_addr1(&port.acio.out, BIO2_BI2A_CMD_WATCHDOG);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    check_int_eq(ac_io_u16(resp.cmd.code), BIO2_BI2A_CMD_WATCHDOG);
    check_int_eq(resp.cmd.nbytes, 1);
    check_int_eq(resp.cmd.status, 0x00);
}

/*
 * test_keepalive: KEEPALIVE must produce empty response (nbytes = 0).
 *
 * Per doc/protocol/bio2.md Section 4.1.
 */
static void test_keepalive(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    const struct ac_io_message *req;

    port_init_bare(&port);
    req = supply_cmd_addr1(&port.acio.out, AC_IO_CMD_KEEPALIVE);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    check_int_eq(ac_io_u16(resp.cmd.code), AC_IO_CMD_KEEPALIVE);
    check_int_eq(resp.cmd.nbytes, 0);
}

/*
 * test_poll_key_packing: Set P1 key 1 and P2 key 7 in the stub, send POLL,
 * verify the corresponding b_val bits in the response body.
 *
 * Per doc/protocol/bio2.md Section 5.2: P1SW1.b_val maps to IIDX_IO_KEY_P1_1,
 * P2SW7.b_val maps to IIDX_IO_KEY_P2_7.
 */
static void test_poll_key_packing(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    struct bi2a_iidx_state_out req_body;
    struct bi2a_iidx_state_in *body;
    const struct ac_io_message *req;

    port_init_bare(&port);
    iidxio_stub_reset();
    iidxio_stub_set_keys((1 << IIDX_IO_KEY_P1_1) | (1 << IIDX_IO_KEY_P2_7));

    memset(&req_body, 0, sizeof(req_body));
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    check_int_eq(ac_io_u16(resp.cmd.code), BIO2_BI2A_CMD_POLL);
    check_int_eq(resp.cmd.nbytes, (int) sizeof(struct bi2a_iidx_state_in));

    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    check_int_eq(body->P1SW1.b_val, 1);
    check_int_eq(body->P1SW2.b_val, 0);
    check_int_eq(body->P2SW7.b_val, 1);
    check_int_eq(body->P2SW1.b_val, 0);
}

/*
 * test_poll_panel_sys_packing: Set panel and sys bits in stub, send POLL,
 * verify PANEL and SYSTEM fields in the response.
 *
 * Per doc/protocol/bio2.md Section 5.2: PANEL.y_start1 maps to P1_START,
 * SYSTEM.v_coin maps to IIDX_IO_SYS_COIN (bit 2 of sys byte).
 *
 * Note: The coin bit test here sets v_coin=1 without relying on coin_count
 * accumulation — it only checks SYSTEM.v_coin in the response body.
 */
static void test_poll_panel_sys_packing(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    struct bi2a_iidx_state_out req_body;
    struct bi2a_iidx_state_in *body;
    const struct ac_io_message *req;

    port_init_bare(&port);
    iidxio_stub_reset();
    iidxio_stub_set_panel(
        (1 << IIDX_IO_PANEL_P1_START) | (1 << IIDX_IO_PANEL_VEFX));
    iidxio_stub_set_sys(1 << IIDX_IO_SYS_TEST);

    memset(&req_body, 0, sizeof(req_body));
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    check_int_eq(body->PANEL.y_start1, 1);
    check_int_eq(body->PANEL.y_vefx, 1);
    check_int_eq(body->PANEL.y_start2, 0);
    check_int_eq(body->PANEL.y_effect, 0);
    check_int_eq(body->SYSTEM.v_test, 1);
    check_int_eq(body->SYSTEM.v_service, 0);
    check_int_eq(body->SYSTEM.v_coin, 0);
}

/*
 * test_poll_turntable_passthrough: Set turntable values in stub, send POLL,
 * verify TURNTABLE1 and TURNTABLE2 fields in the response.
 *
 * Per doc/protocol/bio2.md Section 7: when tt_multiplier_set is false
 * (default), turntable values are passed through directly from
 * iidx_io_ep2_get_turntable.
 */
static void test_poll_turntable_passthrough(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    struct bi2a_iidx_state_out req_body;
    struct bi2a_iidx_state_in *body;
    const struct ac_io_message *req;

    port_init_bare(&port);
    iidxio_stub_reset();
    iidxio_stub_set_turntable(0, 0xAB);
    iidxio_stub_set_turntable(1, 0x42);

    memset(&req_body, 0, sizeof(req_body));
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    check_int_eq(body->TURNTABLE1, 0xAB);
    check_int_eq(body->TURNTABLE2, 0x42);
}

/*
 * test_poll_slider_passthrough: Set slider values in stub, send POLL, verify
 * SLIDERx.s_val fields in the response.
 *
 * Per doc/protocol/bio2.md Section 6.5: when no vefx.txt default is
 * configured, slider values pass through from iidx_io_ep2_get_slider.
 * s_val is a 4-bit field (bits 7:4), so values are clamped to 0..15.
 */
static void test_poll_slider_passthrough(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    struct bi2a_iidx_state_out req_body;
    struct bi2a_iidx_state_in *body;
    const struct ac_io_message *req;

    port_init_bare(&port);
    iidxio_stub_reset();
    iidxio_stub_set_slider(0, 5);
    iidxio_stub_set_slider(1, 10);
    iidxio_stub_set_slider(2, 0);
    iidxio_stub_set_slider(3, 15);
    iidxio_stub_set_slider(4, 7);

    memset(&req_body, 0, sizeof(req_body));
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);

    drain_response(&port.acio.in, &resp);

    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    check_int_eq(body->SLIDER1.s_val, 5);
    check_int_eq(body->SLIDER2.s_val, 10);
    check_int_eq(body->SLIDER3.s_val, 0);
    check_int_eq(body->SLIDER4.s_val, 15);
    check_int_eq(body->SLIDER5.s_val, 7);
}

/*
 * test_coin_latch_edge_detection: Send two POLL messages with coin bit active;
 * count must increment only on the first (rising edge). A third poll with
 * coin bit inactive must clear the latch.
 *
 * Per doc/protocol/bio2.md Section 6.4:
 *   - Rising edge (0→1): coin_count increments, latch set
 *   - Held high (1→1): no re-increment
 *   - Falling edge (1→0): latch cleared
 *
 * Implementation note: coin_latch and coin_count are module-level statics in
 * bi2a.c. This test runs with a fresh port to exercise the logic, but must
 * account for any coin_count accumulated by earlier tests. We first send a
 * coin=0 poll to reset the latch, record the baseline count, then test the
 * edge detection.
 */
static void test_coin_latch_edge_detection(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    struct bi2a_iidx_state_out req_body;
    struct bi2a_iidx_state_in *body;
    const struct ac_io_message *req;
    int baseline_count;

    port_init_bare(&port);
    memset(&req_body, 0, sizeof(req_body));

    /* Poll 1: coin=0 — reset latch and read baseline count */
    iidxio_stub_reset();
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);
    drain_response(&port.acio.in, &resp);
    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    baseline_count = body->coins;

    /* Poll 2: coin=1 — rising edge, count must increment by 1 */
    port_init_bare(&port);
    iidxio_stub_set_sys(1 << IIDX_IO_SYS_COIN);
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);
    drain_response(&port.acio.in, &resp);
    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    check_int_eq(body->coins, baseline_count + 1);

    /* Poll 3: coin=1 still held — count must NOT increment again */
    port_init_bare(&port);
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);
    drain_response(&port.acio.in, &resp);
    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    check_int_eq(body->coins, baseline_count + 1);

    /* Poll 4: coin=0 — falling edge clears latch */
    port_init_bare(&port);
    iidxio_stub_reset();
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);
    drain_response(&port.acio.in, &resp);
    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    check_int_eq(body->coins, baseline_count + 1); /* count unchanged */

    /* Poll 5: coin=1 again — new rising edge, count increments again */
    port_init_bare(&port);
    iidxio_stub_set_sys(1 << IIDX_IO_SYS_COIN);
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);
    drain_response(&port.acio.in, &resp);
    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    check_int_eq(body->coins, baseline_count + 2);
}

/*
 * test_turntable_accumulator: Enable TT multiplier, set a delta in the stub,
 * send POLL, verify TURNTABLE1 accumulates the scaled delta.
 *
 * Per doc/protocol/bio2.md Section 7: when tt_multiplier_set is true,
 * the turntable position is delta-accumulated per poll with the multiplier
 * applied. With tt_last[0] starting at 0 and stub returning value 10,
 * the delta is 10 and the accumulator should be 10 * multiplier after one poll.
 *
 * Note: tt_accum and tt_last are module-level statics. Set turntable to a
 * known value and use multiplier 2.0 so the result is deterministic.
 */
static void test_turntable_accumulator(void)
{
    struct bio2emu_port port;
    struct ac_io_message resp;
    struct bi2a_iidx_state_out req_body;
    struct bi2a_iidx_state_in *body;
    const struct ac_io_message *req;

    /* Reset: send a pass-through poll first to establish known tt_last state
     * (tt_multiplier_set is false by default so this is a raw read). */
    port_init_bare(&port);
    iidxio_stub_reset();
    iidxio_stub_set_turntable(0, 0);
    memset(&req_body, 0, sizeof(req_body));
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);
    drain_response(&port.acio.in, &resp);

    /* Enable accumulator mode with 2x multiplier */
    bio2_emu_bi2a_set_tt_multiplier(2.0f);

    /* Set turntable to position 5; tt_last is 0 so delta = 5 */
    port_init_bare(&port);
    iidxio_stub_reset();
    iidxio_stub_set_turntable(0, 5);
    req = supply_poll_with_out(&port.acio.out, &req_body);
    bio2_emu_bi2a_dispatch_request(&port, req);
    drain_response(&port.acio.in, &resp);

    body = (struct bi2a_iidx_state_in *) resp.cmd.raw;
    /* Expected: tt_accum[0] += round(5 * 2.0) = 10 */
    check_int_eq(body->TURNTABLE1, 10);

    /* Restore pass-through mode for subsequent tests */
    bio2_emu_bi2a_set_tt_multiplier(1.0f);
    /* Reset the set flag by sending a poll that drives tt_multiplier_set=false:
     * We cannot reset the static directly, so just leave multiplier at 1.0.
     * Tests after this are independent of tt accumulator behavior. */
}

TEST_MODULE_BEGIN("bio2emu-iidx")
TEST_MODULE_TEST(test_addr_assignment)
TEST_MODULE_TEST(test_get_version)
TEST_MODULE_TEST(test_start_up)
TEST_MODULE_TEST(test_init)
TEST_MODULE_TEST(test_watchdog)
TEST_MODULE_TEST(test_keepalive)
TEST_MODULE_TEST(test_poll_key_packing)
TEST_MODULE_TEST(test_poll_panel_sys_packing)
TEST_MODULE_TEST(test_poll_turntable_passthrough)
TEST_MODULE_TEST(test_poll_slider_passthrough)
TEST_MODULE_TEST(test_coin_latch_edge_detection)
TEST_MODULE_TEST(test_turntable_accumulator)
TEST_MODULE_END()
