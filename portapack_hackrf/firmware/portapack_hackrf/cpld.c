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

typedef struct jtag_target_t {
	void (*init)();
	void (*free)();
	void (*tck)(const uint_fast8_t value);
	void (*tdi)(const uint_fast8_t value);
	void (*tms)(const uint_fast8_t value);
	uint_fast8_t (*tdo)();
} jtag_target_t;

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

static void portapack_cpld_jtag_init() {
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

static jtag_target_t jtag_target_portapack_cpld = {
	.init = portapack_cpld_jtag_init,
	.free = NULL,
	.tck = portapack_cpld_jtag_tck,
	.tdi = portapack_cpld_jtag_tdi,
	.tms = portapack_cpld_jtag_tms,
	.tdo = portapack_cpld_jtag_tdo,
};

static uint_fast8_t jtag_clock(const jtag_target_t* const target) {
	target->tck(1);
	delay(2);
	target->tck(0);
	delay(2);
	return target->tdo();
}

static void jtag_reset(const jtag_target_t* const target) {
	target->tms(1);
	for(size_t i=0; i<8; i++) {
		jtag_clock(target);
	}
	/* Test-Logic-Reset */
}

void portapack_cpld_jtag_io_init() {
	jtag_target_portapack_cpld.init();
	jtag_reset(&jtag_target_portapack_cpld);
}

#ifdef CPLD_PROGRAM

#include "cpld_data.h"
#include "linux_stuff.h"

static uint_fast8_t jtag_tms_clock(const jtag_target_t* const target, const uint_fast8_t value) {
	target->tms(value);
	return jtag_clock(target);
}

static uint32_t jtag_shift(const jtag_target_t* const target, const uint_fast8_t count, uint32_t value) {
	/* Scan */
	jtag_tms_clock(target, 0);
	/* Capture */
	jtag_tms_clock(target, 0);
	/* Shift */
	for(size_t i=0; i<count; i++) {
		const uint_fast8_t tdo = target->tdo();
		target->tdi(value & 1);
		const uint_fast8_t tms = (i == (count - 1)) ? 1 : 0;
		jtag_tms_clock(target, tms);
		value >>= 1;
		value |= tdo << (count - 1);
		/* Shift or Exit1 */
	}
	jtag_tms_clock(target, 1);
	/* Update */
	jtag_tms_clock(target, 0);
	/* Run-Test/Idle */
	return value;
}

static uint32_t jtag_shift_ir(const jtag_target_t* const target, const uint_fast8_t count, const uint32_t value) {
	/* Run-Test/Idle */
	jtag_tms_clock(target, 1);
	/* Select-DR-Scan */
	jtag_tms_clock(target, 1);
	/* Select-IR-Scan */
	return jtag_shift(target, count, value);
}

static uint32_t jtag_shift_dr(const jtag_target_t* const target, const uint_fast8_t count, const uint32_t value) {
	/* Run-Test/Idle */
	jtag_tms_clock(target, 1);
	/* Select-DR-Scan */
	return jtag_shift(target, count, value);
}

static void jtag_program_block(const jtag_target_t* const target, const uint16_t* const data, const size_t count) {
	for(size_t i=0; i<count; i++) {
		jtag_shift_dr(target, 16, data[i]);
		jtag_runtest_tck(target, 1800);
	}
}

static bool jtag_verify_block(const jtag_target_t* const target, const uint16_t* const data, const size_t count) {
	bool failure = false;
	for(size_t i=0; i<count; i++) {
		const uint16_t from_device = jtag_shift_dr(target, 16, 0xffff);
		if( from_device != data[i] ) {
			failure = true;
		}
	}
	return (failure == false);
}

bool portapack_cpld_jtag_program(const jtag_target_t* const target) {
	/* Unknown state */
	jtag_reset(target);
	/* Test-Logic-Reset */
	jtag_tms_clock(target, 0);
	/* Run-Test/Idle */

	jtag_shift_ir(target, 10, 0b0000000110);
	const uint32_t idcode = jtag_shift_dr(target, 32, 0);
	if( idcode != 0x020A50DD ) {
		return false;
	}

	/* Enter ISP:
	 * Ensures that the I/O pins transition smoothly from user mode to ISP
	 * mode.
	 */
	jtag_shift_ir(target, 10, 0x2cc);
	jtag_runtest_tck(target, 18003);		// 1ms

	/* Check ID:
	 * The silicon ID is checked before any Program or Verify process. The
	 * time required to read this silicon ID is relatively small compared to
	 * the overall programming time.
	 */
	jtag_shift_ir(target, 10, 0x203);
	jtag_runtest_tck(target, 93);		// 5us
	jtag_shift_dr(target, 13, 0x0089);
	jtag_shift_ir(target, 10, 0x205);
	jtag_runtest_tck(target, 93);		// 5us

	uint16_t silicon_id[5];
	silicon_id[0] = jtag_shift_dr(target, 16, 0xffff);
	silicon_id[1] = jtag_shift_dr(target, 16, 0xffff);
	silicon_id[2] = jtag_shift_dr(target, 16, 0xffff);
	silicon_id[3] = jtag_shift_dr(target, 16, 0xffff);
	silicon_id[4] = jtag_shift_dr(target, 16, 0xffff);

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
	jtag_shift_ir(target, 10, 0x203);	// Sector select
	jtag_runtest_tck(target, 93);		// 5us
	jtag_shift_dr(target, 13, 0x0011);	// Sector ID
	jtag_shift_ir(target, 10, 0x2F2);	// Erase pulse
	jtag_runtest_tck(target, 9000003);	// 500ms
	
	jtag_shift_ir(target, 10, 0x203);	// Sector select
	jtag_runtest_tck(target, 93);		// 5us
	jtag_shift_dr(target, 13, 0x0001);	// Sector ID
	jtag_shift_ir(target, 10, 0x2F2);	// Erase pulse
	jtag_runtest_tck(target, 9000003);	// 500ms

	jtag_shift_ir(target, 10, 0x203);	// Sector select
	jtag_runtest_tck(target, 93);		// 5us
	jtag_shift_dr(target, 13, 0x0000);	// Sector ID
	jtag_shift_ir(target, 10, 0x2F2);	// Erase pulse
	jtag_runtest_tck(target, 9000003);	// 500ms

	/* Program:
	 * involves shifting in the address, data, and program instruction and
	 * generating the program pulse to program the flash cells. The program
	 * pulse is automatically generated internally by waiting in the run/test/
	 * idle state for the specified program pulse time of 75 μs. This process
	 * is repeated for each address in the CFM and UFM blocks.
	 */
	/* Block 0 */
	jtag_shift_ir(target, 10, 0x203);	// Sector select
	jtag_runtest_tck(target, 93);		// 5us
	jtag_shift_dr(target, 13, 0x0000);	// Sector ID
	jtag_shift_ir(target, 10, 0x2F4);	// Program
	jtag_runtest_tck(target, 93);		// 5us
	jtag_program_block(
		target,
		portapack_cpld_jtag_block_0,
		ARRAY_SIZE(portapack_cpld_jtag_block_0)
	);

	/* Block 1 */
	jtag_shift_ir(target, 10, 0x203);	// Sector select
	jtag_runtest_tck(target, 93);		// 5us
	jtag_shift_dr(target, 13, 0x0001);	// Sector ID
	jtag_shift_ir(target, 10, 0x2F4);	// Program
	jtag_runtest_tck(target, 93);		// 5us
	jtag_program_block(
		target,
		portapack_cpld_jtag_block_1,
		ARRAY_SIZE(portapack_cpld_jtag_block_1)
	);

	/* Verify */
	/* Block 0 */
	jtag_shift_ir(target, 10, 0x203);	// Sector select
	jtag_runtest_tck(target, 93);		// 5us
	jtag_shift_dr(target, 13, 0x0000);	// Sector ID
	jtag_shift_ir(target, 10, 0x205);	// Read
	jtag_runtest_tck(target, 93);		// 5us
	const bool block_1_success = jtag_verify_block(
		target,
		portapack_cpld_jtag_block_0,
		ARRAY_SIZE(portapack_cpld_jtag_block_0)
	);

	/* Block 1 */
	jtag_shift_ir(target, 10, 0x203);	// Sector select
	jtag_runtest_tck(target, 93);		// 5us
	jtag_shift_dr(target, 13, 0x0001);	// Sector ID
	jtag_shift_ir(target, 10, 0x205);	// Read
	jtag_runtest_tck(target, 93);		// 5us
	const bool block_2_success = jtag_verify_block(
		target,
		portapack_cpld_jtag_block_1,
		ARRAY_SIZE(portapack_cpld_jtag_block_1)
	);

	/* Do "something". Not sure what, but it happens after verify. */
	/* Starts with a sequence the same as Program: Block 0. */
	/* Perhaps it is a write to tell the CPLD that the bitstream
	 * verified OK, and it's OK to load and execute? And despite only
	 * one bit changing, a write must be a multiple of a particular
	 * length (64 bits)? */
	jtag_shift_ir(target, 10, 0x203);	// Sector select
	jtag_runtest_tck(target, 93);		// 5 us
	jtag_shift_dr(target, 13, 0x0000);	// Sector ID
	jtag_shift_ir(target, 10, 0x2F4);	// Program
	jtag_runtest_tck(target, 93);		// 5 us

	/* TODO: Use data from cpld_block_0, with appropriate bit(s) changed */
	/* Perhaps this is the "ISP_DONE" bit? */
	jtag_shift_dr(target, 16, portapack_cpld_jtag_block_0[0] & 0xfbff);
	jtag_runtest_tck(target, 1800);		// 100us
	jtag_shift_dr(target, 16, portapack_cpld_jtag_block_0[1]);
	jtag_runtest_tck(target, 1800);		// 100us
	jtag_shift_dr(target, 16, portapack_cpld_jtag_block_0[2]);
	jtag_runtest_tck(target, 1800);		// 100us
	jtag_shift_dr(target, 16, portapack_cpld_jtag_block_0[3]);
	jtag_runtest_tck(target, 1800);		// 100us

	/* Exit ISP? Reset? */
	jtag_shift_ir(target, 10, 0x201);
	jtag_runtest_tck(target, 18003);		// 1ms
	jtag_shift_ir(target, 10, 0x3FF);
	jtag_runtest_tck(target, 18000);		// 1ms

	if( (block_1_success == false) ||
		(block_2_success == false) ) {
		return false;
	}

	return true;
}
#endif
