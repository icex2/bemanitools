/*
 * ezusb-iidx emulator message dispatch unit tests.
 *
 * Tests derived from doc/protocol/ezusb-iidx.md Sections 3-6 and 11.
 * Uses iidxio-stub for deterministic input state.
 *
 * Coverage:
 *   - Interrupt read: inverted_pad P1 key bits (8-14), active-low
 *   - Interrupt read: inverted_pad P2 key bits (15-21), active-low
 *   - Interrupt read: panel bits (24-27), active-low
 *   - Interrupt read: sys bits (28-30), active-low
 *   - Interrupt read: fpga2_check_flag_unkn always 2
 *   - Interrupt read: fpga_write_ready always 1
 *   - Board type C02: inverted_pad bit 4 unmasked
 *   - Board type D01: inverted_pad bit 4 forced to 0
 *   - Interrupt write: dispatches node command and sets status in next read
 *   - Bulk read: dispatches to cur_node (FPGA V1 node returns valid packet)
 */

#include <stdint.h>
#include <string.h>
#include <windows.h>

#include "ezusb-emu/msg.h"
#include "ezusb-iidx-emu/msg.h"
#include "ezusb-iidx/fpga-cmd.h"
#include "ezusb-iidx/msg.h"
#include "stubs/iidxio/iidxio-stub.h"
#include "test/check.h"
#include "test/test.h"
#include "util/iobuf.h"

/* Helper: perform one interrupt read and return the packet. */
static struct ezusb_iidx_msg_interrupt_read_packet
do_interrupt_read(struct ezusb_emu_msg_hook *hook)
{
    struct ezusb_iidx_msg_interrupt_read_packet pkt;
    uint8_t buf[sizeof(pkt)];
    struct iobuf dest = {buf, sizeof(buf), 0};

    memset(buf, 0, sizeof(buf));

    HRESULT hr = hook->interrupt_read(&dest);
    check_int_eq(hr, S_OK);

    memcpy(&pkt, buf, sizeof(pkt));

    return pkt;
}

/* Helper: perform one interrupt write with given node/cmd fields. */
static void do_interrupt_write(
    struct ezusb_emu_msg_hook *hook,
    uint8_t node,
    uint8_t cmd,
    uint8_t cmd_data,
    uint8_t cmd_data2)
{
    struct ezusb_iidx_msg_interrupt_write_packet req;

    memset(&req, 0, sizeof(req));
    req.node = node;
    req.cmd = cmd;
    req.cmd_detail[0] = cmd_data;
    req.cmd_detail[1] = cmd_data2;
    req.fpga_run = 1;

    struct const_iobuf src = {(const uint8_t *) &req, sizeof(req), 0};

    HRESULT hr = hook->interrupt_write(&src);
    check_int_eq(hr, S_OK);
}

/*
 * test_p1_keys_active_low: Set P1 keys = all pressed (0x7F, bits 0-6).
 * inverted_pad bits 8-14 should all be 0 (active-low = pressed).
 * Per doc/protocol/ezusb-iidx.md Section 4.1.
 */
static void test_p1_keys_active_low(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();
    iidxio_stub_set_keys(0x7F); /* P1 keys 1-7 all pressed */

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    /* P1 keys at bits 8-14: all should be 0 (active-low means pressed = 0) */
    uint32_t p1_bits = (pkt.inverted_pad >> 8) & 0x7F;

    check_int_eq(p1_bits, 0x00);
}

/*
 * test_p1_keys_not_pressed: Set P1 keys = none pressed (0x00).
 * inverted_pad bits 8-14 should all be 1 (active-low = not pressed).
 */
static void test_p1_keys_not_pressed(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();
    iidxio_stub_set_keys(0x00);

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    uint32_t p1_bits = (pkt.inverted_pad >> 8) & 0x7F;

    check_int_eq(p1_bits, 0x7F);
}

/*
 * test_p2_keys_active_low: Set P2 keys = all pressed (bits 7-13 of keys word).
 * inverted_pad bits 15-21 should all be 0 (active-low).
 * Per doc/protocol/ezusb-iidx.md Section 4.1.
 */
static void test_p2_keys_active_low(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();
    iidxio_stub_set_keys(0x3F80); /* P2 keys 1-7 at bits 7-13 */

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    /* P2 keys at bits 15-21 */
    uint32_t p2_bits = (pkt.inverted_pad >> 15) & 0x7F;

    check_int_eq(p2_bits, 0x00);
}

/*
 * test_panel_bits: Set panel = 0x0F (P1 Start + P2 Start + VEFX + Effector).
 * inverted_pad bits 24-27 should be 0 (all panel buttons pressed).
 * Per doc/protocol/ezusb-iidx.md Section 4.1.
 */
static void test_panel_bits(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();
    iidxio_stub_set_panel(0x0F); /* all 4 panel buttons pressed */

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    /* Panel at bits 24-27 */
    uint32_t panel_bits = (pkt.inverted_pad >> 24) & 0x0F;

    check_int_eq(panel_bits, 0x00);
}

/*
 * test_sys_bits: Set sys = 0x03 (Test + Service pressed).
 * inverted_pad bits 28-29 should be 0 (active-low).
 * Per doc/protocol/ezusb-iidx.md Section 4.1.
 */
static void test_sys_bits(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();
    iidxio_stub_set_sys(0x03); /* Test + Service */

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    /* sys bits at 28-30 (3 bits) */
    uint32_t sys_bits = (pkt.inverted_pad >> 28) & 0x07;

    /* bits 28-29 should be 0 (pressed), bit 30 is unset -> stays 1 */
    check_int_eq(sys_bits & 0x03, 0x00);
}

/*
 * test_fpga2_check_flag_always_2: fpga2_check_flag_unkn must be 2 in every
 * interrupt read response. Per doc/protocol/ezusb-iidx.md Section 4.4.
 */
static void test_fpga2_check_flag_always_2(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    check_int_eq(pkt.fpga2_check_flag_unkn, 2);
}

/*
 * test_fpga_write_ready: fpga_write_ready is 1 in every interrupt read.
 * Per doc/protocol/ezusb-iidx.md Section 4.
 */
static void test_fpga_write_ready(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    check_int_eq(pkt.fpga_write_ready, 1);
}

/*
 * test_d01_bit4_forced_zero: On D01 board, inverted_pad bit 4 is forced to 0
 * after the inversion step. Per doc/protocol/ezusb-iidx.md Section 11.
 */
static void test_d01_bit4_forced_zero(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_D01);

    iidxio_stub_reset();

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    check_int_eq((pkt.inverted_pad >> 4) & 0x01, 0);
}

/*
 * test_node_cmd_sets_status: After interrupt write to FPGA V1 node with INIT
 * command, the next interrupt read status byte reflects the command response.
 * Per doc/protocol/ezusb-iidx.md Sections 4.3 and 6.2.
 *
 * FPGA V1 INIT -> status OK_2 (0xFE). Status is cleared after each read.
 */
static void test_node_cmd_sets_status(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();

    /* Send INIT to FPGA V1 node (0x10) */
    do_interrupt_write(
        hook,
        EZUSB_IIDX_MSG_NODE_FPGA_V1,
        EZUSB_IIDX_FPGA_CMD_V1_INIT,
        0x00,
        0x00);

    struct ezusb_iidx_msg_interrupt_read_packet pkt = do_interrupt_read(hook);

    /* FPGA V1 INIT returns OK_2 (0xFE) */
    check_int_eq(pkt.status, EZUSB_IIDX_FPGA_CMD_STATUS_V1_OK_2);
}

/*
 * test_status_cleared_after_read: Status byte is reset to 0 after each
 * interrupt read. Per doc/protocol/ezusb-iidx.md Section 4.3.
 */
static void test_status_cleared_after_read(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();

    do_interrupt_write(
        hook,
        EZUSB_IIDX_MSG_NODE_FPGA_V1,
        EZUSB_IIDX_FPGA_CMD_V1_INIT,
        0x00,
        0x00);

    /* First read: has status */
    do_interrupt_read(hook);

    /* Second read without intervening write: status must be 0 */
    struct ezusb_iidx_msg_interrupt_read_packet pkt2 = do_interrupt_read(hook);

    check_int_eq(pkt2.status, 0x00);
}

/*
 * test_bulk_read_fpga_node: After interrupt write selects FPGA V1 node, bulk
 * read should succeed and return a valid packet.
 * Per doc/protocol/ezusb-iidx.md Section 6.3.
 */
static void test_bulk_read_fpga_node(void)
{
    struct ezusb_emu_msg_hook *hook =
        ezusb_iidx_emu_msg_init(EZUSB_IIDX_EMU_MSG_IO_BOARD_TYPE_C02);

    iidxio_stub_reset();

    /* Select FPGA V1 node */
    do_interrupt_write(
        hook,
        EZUSB_IIDX_MSG_NODE_FPGA_V1,
        EZUSB_IIDX_FPGA_CMD_V1_WRITE,
        0x00,
        0x10);

    /* Bulk read should succeed */
    struct ezusb_iidx_msg_bulk_packet pkt;
    uint8_t buf[sizeof(pkt)];
    struct iobuf dest = {buf, sizeof(buf), 0};

    memset(buf, 0xFF, sizeof(buf));

    HRESULT hr = hook->bulk_read(&dest);
    check_int_eq(hr, S_OK);
    check_int_eq(dest.pos, sizeof(pkt));
}

TEST_MODULE_BEGIN("ezusb-iidx-emu-msg")
TEST_MODULE_TEST(test_p1_keys_active_low)
TEST_MODULE_TEST(test_p1_keys_not_pressed)
TEST_MODULE_TEST(test_p2_keys_active_low)
TEST_MODULE_TEST(test_panel_bits)
TEST_MODULE_TEST(test_sys_bits)
TEST_MODULE_TEST(test_fpga2_check_flag_always_2)
TEST_MODULE_TEST(test_fpga_write_ready)
TEST_MODULE_TEST(test_d01_bit4_forced_zero)
TEST_MODULE_TEST(test_node_cmd_sets_status)
TEST_MODULE_TEST(test_status_cleared_after_read)
TEST_MODULE_TEST(test_bulk_read_fpga_node)
TEST_MODULE_END()
