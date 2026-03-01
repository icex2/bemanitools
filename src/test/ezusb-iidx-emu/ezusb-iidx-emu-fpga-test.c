/*
 * ezusb-iidx emulator FPGA node unit tests.
 *
 * Tests derived from doc/protocol/ezusb-iidx.md Sections 7 and 8.
 * Each test verifies FPGA command/status behavior against documented
 * sequences without depending on internal emulator state.
 *
 * Coverage:
 *   - FPGA V1: INIT, CHECK, CHECK_2, WRITE, WRITE_DONE commands
 *   - FPGA V1: status code values (OK=0x00, OK_2=FAULT=0xFE)
 *   - FPGA V2: INIT, CHECK, WRITE, WRITE_DONE commands
 *   - FPGA V2: distinct status codes (INIT_OK=0x41, CHECK_OK=0x42,
 *              WRITE_OK=0x43, FAULT=0xFE)
 */

#include <stdint.h>

#include "ezusb-iidx-emu/node-fpga.h"
#include "ezusb-iidx/fpga-cmd.h"
#include "test/check.h"
#include "test/test.h"

/*
 * test_fpga_v1_init: INIT command (0x01) -> status OK_2 (0xFE).
 * Per doc/protocol/ezusb-iidx.md Section 7.1.
 */
static void test_fpga_v1_init(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v1_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V1_INIT, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V1_OK_2);
}

/*
 * test_fpga_v1_check: CHECK command (0xFF) -> status OK (0x00).
 * Per doc/protocol/ezusb-iidx.md Section 7.1.
 */
static void test_fpga_v1_check(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v1_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V1_CHECK, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V1_OK);
}

/*
 * test_fpga_v1_check_2: CHECK_2 command (0x02) -> status OK_2 (0xFE).
 * Per doc/protocol/ezusb-iidx.md Section 7.1.
 */
static void test_fpga_v1_check_2(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v1_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V1_CHECK_2, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V1_OK_2);
}

/*
 * test_fpga_v1_write: WRITE command (0x03) -> status OK (0x00).
 * cmd_data and cmd_data2 carry the firmware size big-endian.
 * Per doc/protocol/ezusb-iidx.md Section 7.1.
 */
static void test_fpga_v1_write(void)
{
    /* prog size = 0x1000; cmd_data=0x10, cmd_data2=0x00 */
    uint8_t status = ezusb_iidx_emu_node_fpga_v1_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V1_WRITE, 0x10, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V1_OK);
}

/*
 * test_fpga_v1_write_done: WRITE_DONE command (0x04) -> status OK_2 (0xFE).
 * Per doc/protocol/ezusb-iidx.md Section 7.1.
 */
static void test_fpga_v1_write_done(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v1_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V1_WRITE_DONE, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V1_OK_2);
}

/*
 * test_fpga_v1_unknown_cmd: Unrecognised command -> FAULT (0xFE).
 * Per doc/protocol/ezusb-iidx.md Section 7.2.
 */
static void test_fpga_v1_unknown_cmd(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v1_process_cmd(0xAB, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V1_FAULT);
}

/*
 * test_fpga_v2_init: INIT command (0x01) -> INIT_OK (0x41).
 * Per doc/protocol/ezusb-iidx.md Section 8.1.
 */
static void test_fpga_v2_init(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v2_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V2_INIT, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V2_INIT_OK);
}

/*
 * test_fpga_v2_check: CHECK command (0x02) -> CHECK_OK (0x42).
 * Per doc/protocol/ezusb-iidx.md Section 8.1.
 */
static void test_fpga_v2_check(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v2_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V2_CHECK, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V2_CHECK_OK);
}

/*
 * test_fpga_v2_write: WRITE command (0x03) -> WRITE_OK (0x43).
 * Per doc/protocol/ezusb-iidx.md Section 8.1.
 */
static void test_fpga_v2_write(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v2_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V2_WRITE, 0x10, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V2_WRITE_OK);
}

/*
 * test_fpga_v2_write_done: WRITE_DONE command (0x04) -> WRITE_OK (0x43).
 * Per doc/protocol/ezusb-iidx.md Section 8.1.
 */
static void test_fpga_v2_write_done(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v2_process_cmd(
        EZUSB_IIDX_FPGA_CMD_V2_WRITE_DONE, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V2_WRITE_OK);
}

/*
 * test_fpga_v2_unknown_cmd: Unrecognised command -> FAULT (0xFE).
 * Per doc/protocol/ezusb-iidx.md Section 8.2.
 */
static void test_fpga_v2_unknown_cmd(void)
{
    uint8_t status = ezusb_iidx_emu_node_fpga_v2_process_cmd(0xAB, 0x00, 0x00);

    check_int_eq(status, EZUSB_IIDX_FPGA_CMD_STATUS_V2_FAULT);
}

TEST_MODULE_BEGIN("ezusb-iidx-emu-fpga")
TEST_MODULE_TEST(test_fpga_v1_init)
TEST_MODULE_TEST(test_fpga_v1_check)
TEST_MODULE_TEST(test_fpga_v1_check_2)
TEST_MODULE_TEST(test_fpga_v1_write)
TEST_MODULE_TEST(test_fpga_v1_write_done)
TEST_MODULE_TEST(test_fpga_v1_unknown_cmd)
TEST_MODULE_TEST(test_fpga_v2_init)
TEST_MODULE_TEST(test_fpga_v2_check)
TEST_MODULE_TEST(test_fpga_v2_write)
TEST_MODULE_TEST(test_fpga_v2_write_done)
TEST_MODULE_TEST(test_fpga_v2_unknown_cmd)
TEST_MODULE_END()
