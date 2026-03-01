/*
 * ACIO ICCA card reader node unit tests.
 *
 * Tests derived from doc/protocol/acio.md ICCA node section.
 * Each test verifies ICCA dispatch behavior against documented command
 * semantics. No real hardware is required: eamio-stub provides
 * deterministic card, sensor, and keypad state.
 *
 * Setup pattern:
 *   1. Zero-initialize struct ac_io_emu (no fd needed for dispatch-only).
 *   2. Call ac_io_in_init on emu.in to initialize the response queue.
 *   3. Call ac_io_emu_icca_init with the zeroed emu and unit_no=0.
 *   4. Construct ac_io_message request with the appropriate command code.
 *   5. Call ac_io_emu_icca_dispatch_request.
 *   6. Drain emu.in into a wire buffer; re-parse with ac_io_out to recover
 *      the response ac_io_message for field assertions.
 */

#include <stdint.h>
#include <string.h>

#include "acio/acio.h"
#include "acio/icca.h"
#include "acioemu/emu.h"
#include "acioemu/icca.h"
#include "acioemu/pipe.h"
#include "bemanitools/eamio.h"
#include "stubs/eamio/eamio-stub.h"
#include "test/check.h"
#include "test/test.h"
#include "util/iobuf.h"

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/*
 * dispatch_and_recv: Send one request to the ICCA node and receive the
 * response. Drains emu.in into wire_buf, then re-parses to recover the
 * decoded ac_io_message. Returns pointer to the internal buffer of out
 * (valid until out is reused).
 */
static const struct ac_io_message *dispatch_and_recv(
    struct ac_io_emu_icca *icca,
    struct ac_io_emu *emu,
    const struct ac_io_message *req,
    uint8_t *wire_buf,
    size_t wire_buf_sz,
    struct ac_io_out *out)
{
    struct iobuf dest;
    struct const_iobuf src;

    ac_io_emu_icca_dispatch_request(icca, req);

    dest.bytes = wire_buf;
    dest.nbytes = wire_buf_sz;
    dest.pos = 0;

    ac_io_in_drain(&emu->in, &dest);

    src.bytes = wire_buf;
    src.nbytes = dest.pos;
    src.pos = 0;

    ac_io_out_init(out);
    ac_io_out_supply(out, &src);

    if (!ac_io_out_have_message(out)) {
        return NULL;
    }

    return ac_io_out_get_message(out);
}

/*
 * make_req: Build a minimal ac_io_message request with the given address,
 * command code, seq_no, and zero payload.
 */
static void
make_req(struct ac_io_message *req, uint8_t addr, uint16_t cmd, uint8_t seq_no)
{
    memset(req, 0, sizeof(*req));
    req->addr = addr;
    req->cmd.code = ac_io_u16(cmd);
    req->cmd.seq_no = seq_no;
    req->cmd.nbytes = 0;
}

/* -------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------- */

/*
 * test_get_version: GET_VERSION response must report node type ICCA
 * (0x03000000) with the default version v160 (major=1, minor=6).
 */
static void test_get_version(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;
    const struct ac_io_version *ver;

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);

    make_req(&req, 0x01, AC_IO_CMD_GET_VERSION, 0x00);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    check_int_eq(resp->addr, (uint8_t) (0x01 | AC_IO_RESPONSE_FLAG));
    check_int_eq(ac_io_u16(resp->cmd.code), AC_IO_CMD_GET_VERSION);
    check_int_eq(resp->cmd.nbytes, sizeof(struct ac_io_version));

    ver = &resp->cmd.version;
    check_int_eq(ac_io_u32(ver->type), AC_IO_NODE_TYPE_ICCA);
    check_int_eq(ver->major, 0x01);
    check_int_eq(ver->minor, 0x06); /* v160 default */
}

/*
 * test_get_version_v150: Override version to v150; minor must be 5.
 */
static void test_get_version_v150(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);
    ac_io_emu_icca_set_version(&icca, v150);

    make_req(&req, 0x01, AC_IO_CMD_GET_VERSION, 0x00);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    check_int_eq(resp->cmd.version.minor, 0x05);
}

/*
 * test_get_version_v170: Override version to v170; minor must be 7.
 */
static void test_get_version_v170(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);
    ac_io_emu_icca_set_version(&icca, v170);

    make_req(&req, 0x01, AC_IO_CMD_GET_VERSION, 0x00);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    check_int_eq(resp->cmd.version.minor, 0x07);
}

/*
 * test_start_up: START_UP response status must be 0x00.
 * After init, fault=true; START_UP does not change fault itself but responds
 * with status 0x00 indicating no error in the startup command.
 */
static void test_start_up(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);

    make_req(&req, 0x01, AC_IO_CMD_START_UP, 0x01);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    check_int_eq(resp->addr, (uint8_t) (0x01 | AC_IO_RESPONSE_FLAG));
    check_int_eq(ac_io_u16(resp->cmd.code), AC_IO_CMD_START_UP);
    check_int_eq(resp->cmd.nbytes, sizeof(resp->cmd.status));
    check_int_eq(resp->cmd.status, 0x00);
}

/*
 * test_queue_loop_start: QUEUE_LOOP_START response status must be 0x00 and
 * must set polling_started=true, clearing the fault state.
 */
static void test_queue_loop_start(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);

    /* fault starts true after init */
    check_bool_true(icca.fault);
    check_bool_false(icca.polling_started);

    make_req(&req, 0x01, AC_IO_ICCA_CMD_QUEUE_LOOP_START, 0x00);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    check_int_eq(resp->cmd.status, 0x00);
    check_bool_false(icca.fault);
    check_bool_true(icca.polling_started);
}

/*
 * test_poll_before_queue_loop: POLL before queue loop is started must return
 * status_code AC_IO_ICCA_STATUS_FAULT (0x00) — the polling guard fires
 * regardless of sensor state.
 */
static void test_poll_before_queue_loop(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;
    const struct ac_io_icca_state *body;

    eamio_stub_reset();

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);

    /* Do NOT call QUEUE_LOOP_START — polling_started stays false */

    make_req(&req, 0x01, AC_IO_ICCA_CMD_POLL, 0x00);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    check_int_eq(ac_io_u16(resp->cmd.code), AC_IO_ICCA_CMD_POLL);
    check_int_eq(resp->cmd.nbytes, sizeof(struct ac_io_icca_state));

    body = (const struct ac_io_icca_state *) &resp->cmd.raw;
    check_int_eq(body->status_code, AC_IO_ICCA_STATUS_FAULT);
}

/*
 * test_poll_idle: After QUEUE_LOOP_START with no card inserted, POLL
 * returns status_code IDLE (0x01).
 */
static void test_poll_idle(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;
    const struct ac_io_icca_state *body;

    eamio_stub_reset();

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);

    /* Start queue loop to clear fault */
    make_req(&req, 0x01, AC_IO_ICCA_CMD_QUEUE_LOOP_START, 0x00);
    dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    /* No card: sensor state = 0 */
    eamio_stub_set_sensor(0, 0x00);

    make_req(&req, 0x01, AC_IO_ICCA_CMD_POLL, 0x01);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    body = (const struct ac_io_icca_state *) &resp->cmd.raw;
    check_int_eq(body->status_code, AC_IO_ICCA_STATUS_IDLE);
}

/*
 * test_poll_card_insert: Set both front and back sensors, configure card data
 * in eamio-stub, then POLL. Response must show GOT_UID (0x02) with matching
 * UID bytes.
 */
static void test_poll_card_insert(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;
    const struct ac_io_icca_state *body;
    static const uint8_t card_id[8] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t front_back_sensor =
        (1 << EAM_IO_SENSOR_FRONT) | (1 << EAM_IO_SENSOR_BACK);

    eamio_stub_reset();
    eamio_stub_set_sensor(0, front_back_sensor);
    eamio_stub_set_card(0, EAM_IO_CARD_ISO15696, card_id, sizeof(card_id));

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);

    /* Start queue loop */
    make_req(&req, 0x01, AC_IO_ICCA_CMD_QUEUE_LOOP_START, 0x00);
    dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    /* Poll with card fully inserted */
    make_req(&req, 0x01, AC_IO_ICCA_CMD_POLL, 0x01);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    body = (const struct ac_io_icca_state *) &resp->cmd.raw;
    check_int_eq(body->status_code, AC_IO_ICCA_STATUS_GOT_UID);
    check_data_eq(body->uid, sizeof(body->uid), card_id, sizeof(card_id));
}

/*
 * test_poll_keypad_event: Set a keypad bit in eamio-stub, poll, verify
 * key_events[0] is populated (non-zero) indicating a keypad press.
 */
static void test_poll_keypad_event(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;
    const struct ac_io_icca_state *body;

    eamio_stub_reset();

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);

    /* Start queue loop */
    make_req(&req, 0x01, AC_IO_ICCA_CMD_QUEUE_LOOP_START, 0x00);
    dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    /* First poll: no keys, establishes last_keypad baseline */
    eamio_stub_set_keypad(0, 0x0000);
    make_req(&req, 0x01, AC_IO_ICCA_CMD_POLL, 0x01);
    dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    /* Second poll: key "1" pressed (bit 1 = EAM_IO_KEYPAD_1) */
    eamio_stub_set_keypad(0, 1 << EAM_IO_KEYPAD_1);
    make_req(&req, 0x01, AC_IO_ICCA_CMD_POLL, 0x02);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    body = (const struct ac_io_icca_state *) &resp->cmd.raw;
    /* key_events[0] must be non-zero: bit 7 set (key-press) + bit index */
    check_bool_true(body->key_events[0] != 0);
    /* key_state must reflect current keypad bitfield */
    check_bool_true(ac_io_u16(body->key_state) != 0);
}

/*
 * test_poll_felica_v150: On v150, POLL_FELICA behaves like a regular POLL
 * (returns ac_io_icca_state).
 */
static void test_poll_felica_v150(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;

    eamio_stub_reset();

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);
    ac_io_emu_icca_set_version(&icca, v150);

    /* Start queue loop */
    make_req(&req, 0x01, AC_IO_ICCA_CMD_QUEUE_LOOP_START, 0x00);
    dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    make_req(&req, 0x01, AC_IO_ICCA_CMD_POLL_FELICA, 0x01);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    /* v150: POLL_FELICA calls send_state -> nbytes = sizeof(ac_io_icca_state)
     */
    check_int_eq(resp->cmd.nbytes, sizeof(struct ac_io_icca_state));
}

/*
 * test_poll_felica_v160: On v160, POLL_FELICA returns status 0x01 and sets
 * detected_new_reader.
 */
static void test_poll_felica_v160(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;

    eamio_stub_reset();

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);
    /* v160 is default, but set explicitly for clarity */
    ac_io_emu_icca_set_version(&icca, v160);

    check_bool_false(icca.detected_new_reader);

    make_req(&req, 0x01, AC_IO_ICCA_CMD_POLL_FELICA, 0x01);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    /* v160: send_status(0x01) -> status byte only */
    check_int_eq(resp->cmd.nbytes, sizeof(resp->cmd.status));
    check_int_eq(resp->cmd.status, 0x01);
    check_bool_true(icca.detected_new_reader);
}

/*
 * test_poll_felica_v170: On v170, POLL_FELICA sends an empty response
 * (nbytes=0) and sets detected_new_reader.
 */
static void test_poll_felica_v170(void)
{
    struct ac_io_emu emu;
    struct ac_io_emu_icca icca;
    struct ac_io_message req;
    struct ac_io_out out;
    uint8_t wire[512];
    const struct ac_io_message *resp;

    eamio_stub_reset();

    memset(&emu, 0, sizeof(emu));
    ac_io_in_init(&emu.in);
    ac_io_emu_icca_init(&icca, &emu, 0);
    ac_io_emu_icca_set_version(&icca, v170);

    check_bool_false(icca.detected_new_reader);

    make_req(&req, 0x01, AC_IO_ICCA_CMD_POLL_FELICA, 0x01);

    resp = dispatch_and_recv(&icca, &emu, &req, wire, sizeof(wire), &out);

    check_non_null(resp);
    /* v170: send_empty -> nbytes=0 */
    check_int_eq(resp->cmd.nbytes, 0);
    check_bool_true(icca.detected_new_reader);
}

TEST_MODULE_BEGIN("acioemu-icca")
TEST_MODULE_TEST(test_get_version)
TEST_MODULE_TEST(test_get_version_v150)
TEST_MODULE_TEST(test_get_version_v170)
TEST_MODULE_TEST(test_start_up)
TEST_MODULE_TEST(test_queue_loop_start)
TEST_MODULE_TEST(test_poll_before_queue_loop)
TEST_MODULE_TEST(test_poll_idle)
TEST_MODULE_TEST(test_poll_card_insert)
TEST_MODULE_TEST(test_poll_keypad_event)
TEST_MODULE_TEST(test_poll_felica_v150)
TEST_MODULE_TEST(test_poll_felica_v160)
TEST_MODULE_TEST(test_poll_felica_v170)
TEST_MODULE_END()
