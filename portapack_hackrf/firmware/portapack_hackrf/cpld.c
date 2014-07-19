/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <stddef.h>

#include <delay.h>

#include <libopencm3/lpc43xx/gpio.h>
#include <libopencm3/lpc43xx/scu.h>
 
/*
typedef enum {
	JTAG_TAP_STATE_RESET,
	JTAG_TAP_STATE_IDLE,
	JTAG_TAP_STATE_DRSELECT,
	JTAG_TAP_STATE_DRCAPTURE,
	JTAG_TAP_STATE_DRSHIFT,
	JTAG_TAP_STATE_DRPAUSE,
	JTAG_TAP_STATE_DREXIT1,
	JTAG_TAP_STATE_DREXIT2,
	JTAG_TAP_STATE_DRUPDATE,
	JTAG_TAP_STATE_IRSELECT,
	JTAG_TAP_STATE_IRCAPTURE,
	JTAG_TAP_STATE_IRSHIFT,
	JTAG_TAP_STATE_IRPAUSE,
	JTAG_TAP_STATE_IREXIT1,
	JTAG_TAP_STATE_IREXIT2,
	JTAG_TAP_STATE_IRUPDATE,
} jtag_tap_state_t;

typedef struct jtag_svf_t {
	jtag_tap_state_t state;
	jtag_tap_state_t end_dr_state;
	jtag_tap_state_t end_ir_state;
} jtag_svf_t;

void jtag_set_state(jtag_svf_t* const svf, const jtag_tap_state_t new_state) {

}

void jtag_set_end_dr_state(jtag_svf_t* const svf, const jtag_tap_state_t state) {
	jtag_svf->end_dr_state = state;
}

void jtag_set_end_ir_state(jtag_svf_t* const svf, const jtag_tap_state_t state) {
	jtag_svf->end_ir_state = state;
}
*/

#define PORTAPACK_CPLD_JTAG_TDI_SCU_PIN (P6_2)
#define PORTAPACK_CPLD_JTAG_TDI_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_CPLD_JTAG_TDI_GPIO_PORT (GPIO3)
#define PORTAPACK_CPLD_JTAG_TDI_GPIO_PIN (1)
#define PORTAPACK_CPLD_JTAG_TDI_GPIO_BIT (1UL << PORTAPACK_CPLD_JTAG_TDI_GPIO_PIN)

#define PORTAPACK_CPLD_JTAG_TMS_SCU_PIN (P1_8)
#define PORTAPACK_CPLD_JTAG_TMS_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_CPLD_JTAG_TMS_GPIO_PORT (GPIO1)
#define PORTAPACK_CPLD_JTAG_TMS_GPIO_PIN (1)
#define PORTAPACK_CPLD_JTAG_TMS_GPIO_BIT (1UL << PORTAPACK_CPLD_JTAG_TMS_GPIO_PIN)

#define PORTAPACK_CPLD_JTAG_TCK_SCU_PIN (P6_1)
#define PORTAPACK_CPLD_JTAG_TCK_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_CPLD_JTAG_TCK_GPIO_PORT (GPIO3)
#define PORTAPACK_CPLD_JTAG_TCK_GPIO_PIN (0)
#define PORTAPACK_CPLD_JTAG_TCK_GPIO_BIT (1UL << PORTAPACK_CPLD_JTAG_TCK_GPIO_PIN)

#define PORTAPACK_CPLD_JTAG_TDO_SCU_PIN (P1_5)
#define PORTAPACK_CPLD_JTAG_TDO_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_CPLD_JTAG_TDO_GPIO_PORT (GPIO1)
#define PORTAPACK_CPLD_JTAG_TDO_GPIO_PIN (8)
#define PORTAPACK_CPLD_JTAG_TDO_GPIO_BIT (1UL << PORTAPACK_CPLD_JTAG_TDO_GPIO_PIN)

static void portapack_cpld_jtag_tck(const uint_fast8_t value) {
	if(value) {
		gpio_set(PORTAPACK_CPLD_JTAG_TCK_GPIO_PORT, PORTAPACK_CPLD_JTAG_TCK_GPIO_BIT);
	} else {
		gpio_clear(PORTAPACK_CPLD_JTAG_TCK_GPIO_PORT, PORTAPACK_CPLD_JTAG_TCK_GPIO_BIT);
	}
}

static void portapack_cpld_jtag_tdi(const uint_fast8_t value) {
	if(value) {
		gpio_set(PORTAPACK_CPLD_JTAG_TDI_GPIO_PORT, PORTAPACK_CPLD_JTAG_TDI_GPIO_BIT);
	} else {
		gpio_clear(PORTAPACK_CPLD_JTAG_TDI_GPIO_PORT, PORTAPACK_CPLD_JTAG_TDI_GPIO_BIT);
	}
}

static void portapack_cpld_jtag_tms(const uint_fast8_t value) {
	if(value) {
		gpio_set(PORTAPACK_CPLD_JTAG_TMS_GPIO_PORT, PORTAPACK_CPLD_JTAG_TMS_GPIO_BIT);
	} else {
		gpio_clear(PORTAPACK_CPLD_JTAG_TMS_GPIO_PORT, PORTAPACK_CPLD_JTAG_TMS_GPIO_BIT);
	}
}

static uint_fast8_t portapack_cpld_jtag_tdo() {
	//return (GPIO_PIN(PORTAPACK_CPLD_JTAG_TDO_GPIO_PORT) >> PORTAPACK_CPLD_JTAG_TMS_GPIO_PIN) & 1;
	return gpio_get(PORTAPACK_CPLD_JTAG_TDO_GPIO_PORT, PORTAPACK_CPLD_JTAG_TDO_GPIO_BIT);
}

static uint_fast8_t portapack_cpld_jtag_clock() {
	portapack_cpld_jtag_tck(1);
	delay(2);
	portapack_cpld_jtag_tck(0);
	delay(2);
	return portapack_cpld_jtag_tdo();
}

void portapack_cpld_jtag_reset() {
	portapack_cpld_jtag_tms(1);
	for(size_t i=0; i<8; i++) {
		portapack_cpld_jtag_clock();
	}
	/* Test-Logic-Reset */
}

void portapack_cpld_jtag_io_init() {
	/* Configure pins to GPIO */
	scu_pinmux(PORTAPACK_CPLD_JTAG_TDI_SCU_PIN, SCU_GPIO_PUP | PORTAPACK_CPLD_JTAG_TDI_SCU_FUNCTION);
	scu_pinmux(PORTAPACK_CPLD_JTAG_TMS_SCU_PIN, SCU_GPIO_PUP | PORTAPACK_CPLD_JTAG_TMS_SCU_FUNCTION);
	scu_pinmux(PORTAPACK_CPLD_JTAG_TCK_SCU_PIN, SCU_GPIO_PDN | PORTAPACK_CPLD_JTAG_TCK_SCU_FUNCTION);
	scu_pinmux(PORTAPACK_CPLD_JTAG_TDO_SCU_PIN, SCU_GPIO_NOPULL | PORTAPACK_CPLD_JTAG_TDO_SCU_FUNCTION);

	portapack_cpld_jtag_tdi(1);
	portapack_cpld_jtag_tms(1);
	portapack_cpld_jtag_tck(0);

	/* Configure pin directions */
	GPIO_DIR(PORTAPACK_CPLD_JTAG_TDI_GPIO_PORT) |= PORTAPACK_CPLD_JTAG_TDI_GPIO_BIT;
	GPIO_DIR(PORTAPACK_CPLD_JTAG_TMS_GPIO_PORT) |= PORTAPACK_CPLD_JTAG_TMS_GPIO_BIT;
	GPIO_DIR(PORTAPACK_CPLD_JTAG_TCK_GPIO_PORT) |= PORTAPACK_CPLD_JTAG_TCK_GPIO_BIT;
	GPIO_DIR(PORTAPACK_CPLD_JTAG_TDO_GPIO_PORT) &= ~PORTAPACK_CPLD_JTAG_TDO_GPIO_BIT;
}

#ifdef CPLD_PROGRAM

#include "cpld_data.h"
#include "linux_stuff.h"

static uint_fast8_t portapack_cpld_jtag_tms_clock(const uint_fast8_t value) {
	portapack_cpld_jtag_tms(value);
	return portapack_cpld_jtag_clock();
}

static uint32_t portapack_cpld_jtag_shift(const uint_fast8_t count, uint32_t value) {
	/* Scan */
	portapack_cpld_jtag_tms_clock(0);
	/* Capture */
	portapack_cpld_jtag_tms_clock(0);
	/* Shift */
	for(size_t i=0; i<count; i++) {
		const uint_fast8_t tdo = portapack_cpld_jtag_tdo();
		portapack_cpld_jtag_tdi(value & 1);
		const uint_fast8_t tms = (i == (count - 1)) ? 1 : 0;
		portapack_cpld_jtag_tms_clock(tms);
		value >>= 1;
		value |= tdo << (count - 1);
		/* Shift or Exit1 */
	}
	portapack_cpld_jtag_tms_clock(1);
	/* Update */
	portapack_cpld_jtag_tms_clock(0);
	/* Run-Test/Idle */
	return value;
}

static uint32_t portapack_cpld_jtag_shift_ir(const uint_fast8_t count, const uint32_t value) {
	/* Run-Test/Idle */
	portapack_cpld_jtag_tms_clock(1);
	/* Select-DR-Scan */
	portapack_cpld_jtag_tms_clock(1);
	/* Select-IR-Scan */
	return portapack_cpld_jtag_shift(count, value);
}

static uint32_t portapack_cpld_jtag_shift_dr(const uint_fast8_t count, const uint32_t value) {
	/* Run-Test/Idle */
	portapack_cpld_jtag_tms_clock(1);
	/* Select-DR-Scan */
	return portapack_cpld_jtag_shift(count, value);
}

static void portapack_cpld_jtag_program_block(const uint16_t* const data, const size_t count) {
	for(size_t i=0; i<count; i++) {
		portapack_cpld_jtag_shift_dr(16, data[i]);
		portapack_cpld_jtag_runtest_tck(1800);
	}
}

static bool portapack_cpld_jtag_verify_block(const uint16_t* const data, const size_t count) {
	bool failure = false;
	for(size_t i=0; i<count; i++) {
		const uint16_t from_device = portapack_cpld_jtag_shift_dr(16, 0xffff);
		if( from_device != data[i] ) {
			failure = true;
		}
	}
	return (failure == false);
}

bool portapack_cpld_jtag_program() {
	/* Unknown state */
	portapack_cpld_jtag_reset();
	/* Test-Logic-Reset */
	portapack_cpld_jtag_tms_clock(0);
	/* Run-Test/Idle */

	portapack_cpld_jtag_shift_ir(10, 0b0000000110);
	const uint32_t idcode = portapack_cpld_jtag_shift_dr(32, 0);
	if( idcode != 0x020A50DD ) {
		return false;
	}

	/* Enter ISP:
	 * Ensures that the I/O pins transition smoothly from user mode to ISP
	 * mode.
	 */
	portapack_cpld_jtag_shift_ir(10, 0x2cc);
	portapack_cpld_jtag_runtest_tck(18003);		// 1ms

	/* Check ID:
	 * The silicon ID is checked before any Program or Verify process. The
	 * time required to read this silicon ID is relatively small compared to
	 * the overall programming time.
	 */
	portapack_cpld_jtag_shift_ir(10, 0x203);
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_shift_dr(13, 0x0089);
	portapack_cpld_jtag_shift_ir(10, 0x205);
	portapack_cpld_jtag_runtest_tck(93);		// 5us

	uint16_t silicon_id[5];
	silicon_id[0] = portapack_cpld_jtag_shift_dr(16, 0xffff);
	silicon_id[1] = portapack_cpld_jtag_shift_dr(16, 0xffff);
	silicon_id[2] = portapack_cpld_jtag_shift_dr(16, 0xffff);
	silicon_id[3] = portapack_cpld_jtag_shift_dr(16, 0xffff);
	silicon_id[4] = portapack_cpld_jtag_shift_dr(16, 0xffff);

	if( (silicon_id[0] != 0x8232) ||
		(silicon_id[1] != 0x2aa2) ||
		(silicon_id[2] != 0x4a82) ||
		(silicon_id[3] != 0x8c0c) ||
		(silicon_id[4] != 0x0000) ) {
		return false;
	}

	/* Sector erase:
	 * Involves shifting in the instruction to erase the device and applying
	 * an erase pulse or pulses. The erase pulse is automatically generated
	 * internally by waiting in the run, test, or idle state for the
	 * specified erase pulse time of 500 ms for the CFM block and 500 ms for
	 * each sector of the user flash memory (UFM) block.
	 */
	portapack_cpld_jtag_shift_ir(10, 0x203);	// Sector select
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_shift_dr(13, 0x0011);	// Sector ID
	portapack_cpld_jtag_shift_ir(10, 0x2F2);	// Erase pulse
	portapack_cpld_jtag_runtest_tck(9000003);	// 500ms
	
	portapack_cpld_jtag_shift_ir(10, 0x203);	// Sector select
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_shift_dr(13, 0x0001);	// Sector ID
	portapack_cpld_jtag_shift_ir(10, 0x2F2);	// Erase pulse
	portapack_cpld_jtag_runtest_tck(9000003);	// 500ms

	portapack_cpld_jtag_shift_ir(10, 0x203);	// Sector select
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_shift_dr(13, 0x0000);	// Sector ID
	portapack_cpld_jtag_shift_ir(10, 0x2F2);	// Erase pulse
	portapack_cpld_jtag_runtest_tck(9000003);	// 500ms

	/* Program:
	 * involves shifting in the address, data, and program instruction and
	 * generating the program pulse to program the flash cells. The program
	 * pulse is automatically generated internally by waiting in the run/test/
	 * idle state for the specified program pulse time of 75 μs. This process
	 * is repeated for each address in the CFM and UFM blocks.
	 */
	/* Block 0 */
	portapack_cpld_jtag_shift_ir(10, 0x203);	// Sector select
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_shift_dr(13, 0x0000);	// Sector ID
	portapack_cpld_jtag_shift_ir(10, 0x2F4);	// Program
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_program_block(
		portapack_cpld_jtag_block_0,
		ARRAY_SIZE(portapack_cpld_jtag_block_0)
	);

	/* Block 1 */
	portapack_cpld_jtag_shift_ir(10, 0x203);	// Sector select
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_shift_dr(13, 0x0001);	// Sector ID
	portapack_cpld_jtag_shift_ir(10, 0x2F4);	// Program
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_program_block(
		portapack_cpld_jtag_block_1,
		ARRAY_SIZE(portapack_cpld_jtag_block_1)
	);

	/* Verify */
	/* Block 0 */
	portapack_cpld_jtag_shift_ir(10, 0x203);	// Sector select
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_shift_dr(13, 0x0000);	// Sector ID
	portapack_cpld_jtag_shift_ir(10, 0x205);	// Read
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	const bool block_1_success = portapack_cpld_jtag_verify_block(
		portapack_cpld_jtag_block_0,
		ARRAY_SIZE(portapack_cpld_jtag_block_0)
	);

	/* Block 1 */
	portapack_cpld_jtag_shift_ir(10, 0x203);	// Sector select
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	portapack_cpld_jtag_shift_dr(13, 0x0001);	// Sector ID
	portapack_cpld_jtag_shift_ir(10, 0x205);	// Read
	portapack_cpld_jtag_runtest_tck(93);		// 5us
	const bool block_2_success = portapack_cpld_jtag_verify_block(
		portapack_cpld_jtag_block_1,
		ARRAY_SIZE(portapack_cpld_jtag_block_1)
	);

	/* Do "something". Not sure what, but it happens after verify. */
	/* Starts with a sequence the same as Program: Block 0. */
	/* Perhaps it is a write to tell the CPLD that the bitstream
	 * verified OK, and it's OK to load and execute? And despite only
	 * one bit changing, a write must be a multiple of a particular
	 * length (64 bits)? */
	portapack_cpld_jtag_shift_ir(10, 0x203);	// Sector select
	portapack_cpld_jtag_runtest_tck(93);		// 5 us
	portapack_cpld_jtag_shift_dr(13, 0x0000);	// Sector ID
	portapack_cpld_jtag_shift_ir(10, 0x2F4);	// Program
	portapack_cpld_jtag_runtest_tck(93);		// 5 us

	/* TODO: Use data from cpld_block_0, with appropriate bit(s) changed */
	/* Perhaps this is the "ISP_DONE" bit? */
	portapack_cpld_jtag_shift_dr(16, portapack_cpld_jtag_block_0[0] & 0xfbff);
	portapack_cpld_jtag_runtest_tck(1800);		// 100us
	portapack_cpld_jtag_shift_dr(16, portapack_cpld_jtag_block_0[1]);
	portapack_cpld_jtag_runtest_tck(1800);		// 100us
	portapack_cpld_jtag_shift_dr(16, portapack_cpld_jtag_block_0[2]);
	portapack_cpld_jtag_runtest_tck(1800);		// 100us
	portapack_cpld_jtag_shift_dr(16, portapack_cpld_jtag_block_0[3]);
	portapack_cpld_jtag_runtest_tck(1800);		// 100us

	/* Exit ISP? Reset? */
	portapack_cpld_jtag_shift_ir(10, 0x201);
	portapack_cpld_jtag_runtest_tck(18003);		// 1ms
	portapack_cpld_jtag_shift_ir(10, 0x3FF);
	portapack_cpld_jtag_runtest_tck(18000);		// 1ms

	if( (block_1_success == false) ||
		(block_2_success == false) ) {
		return false;
	}

	return true;
}
#endif
