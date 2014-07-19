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

#include "sdio.h"

#include <stdint.h>
#include <stdbool.h>

#include <libopencm3/lpc43xx/cgu.h>
#include <libopencm3/lpc43xx/sdio.h>

#define SDIO_CMD0_INDEX (0b000000)

#define SDIO_CMD2_INDEX (0b000010)

#define SDIO_CMD3_INDEX (0b000011)

#define SDIO_CMD5_INDEX (0b000101)
#define SDIO_CMD5_S18R(x) ((x) << 24)
#define SDIO_CMD5_IO_OCR(x) ((x) << 0)
#define SDIO_CMD5_IO_OCR_BIT(n) SDIO_CMD5_IO_OCR((1 << (n)))

#define SDIO_CMD8_INDEX (0b001000)

#define SDIO_CMD55_INDEX (0b110111)

#define SDIO_ACMD41_INDEX (0b101001)

static void sdio_controller_reset() {
	//RESET_CTRL0 = RESET_CTRL0_SDIO_RST;
	/*
	*/
	SDIO_BMOD = SDIO_BMOD_SWR(1);
	SDIO_CTRL =
		  SDIO_CTRL_CONTROLLER_RESET(1)
		| SDIO_CTRL_FIFO_RESET(1)
		| SDIO_CTRL_DMA_RESET(1)
		;
	while( SDIO_CTRL & (SDIO_CTRL_CONTROLLER_RESET(1) | SDIO_CTRL_FIFO_RESET(1) | SDIO_CTRL_DMA_RESET(1)) );
}

static void sdio_clear_interrupts() {
	SDIO_RINTSTS = 0xffffffff;			
}

static void sdio_wait_for_command_accepted() {
	while( (SDIO_CMD & SDIO_CMD_START_CMD(1)) != 0 );
}

static void sdio_update_clock_registers_only() {
	SDIO_CMDARG = 0;
	SDIO_CMD =
		  SDIO_CMD_UPDATE_CLOCK_REGISTERS_ONLY(1)
		| SDIO_CMD_WAIT_PRVDATA_COMPLETE(1)
		| SDIO_CMD_START_CMD(1)
		;
	sdio_wait_for_command_accepted();
}

void sdio_cclk_set_400khz() {
	/* "Before issuing a new data transfer command, the software should
	 * ensure that the card is not busy due to any previous data
	 * transfer command. Before changing the card clock frequency, the
	 * software must ensure that there are no data or command transfers
	 * in progress."
	 */
	/* "To avoid glitches in the card clock outputs (cclk_out), the
	 * software should use the following steps when changing the card
	 * clock frequency:"
	 */
	SDIO_CLKENA &= ~SDIO_CLKENA_CCLK_ENABLE(1);
	SDIO_CLKSRC = SDIO_CLKSRC_CLK_SOURCE(0);
	sdio_update_clock_registers_only();

	SDIO_CLKDIV =
		  SDIO_CLKDIV_CLK_DIVIDER0(0xff)
		| SDIO_CLKDIV_CLK_DIVIDER1(0)
		| SDIO_CLKDIV_CLK_DIVIDER2(0)
		| SDIO_CLKDIV_CLK_DIVIDER3(0)
		;
	sdio_update_clock_registers_only();

	SDIO_CLKENA |= SDIO_CLKENA_CCLK_ENABLE(1);
	sdio_update_clock_registers_only();
}

void sdio_set_width_1bit() {
	SDIO_CTYPE =
		  SDIO_CTYPE_CARD_WIDTH0(0)
		| SDIO_CTYPE_CARD_WIDTH1(0)
		;
}

bool sdio_card_is_present() {
	return (SDIO_CDETECT & SDIO_CDETECT_CARD_DETECT(1)) ? false : true;
}

static bool sdio_error_hardware_is_locked(const uint32_t status) {
	return (status & SDIO_RINTSTS_HLE(1)) ? true : false;
}

static bool sdio_command_is_complete(const uint32_t status) {
	return (status & SDIO_RINTSTS_CDONE(1)) ? true : false;
}

static bool sdio_response_error(const uint32_t status) {
	return (status & SDIO_RINTSTS_RE(1)) ? true : false;
}

static bool sdio_response_crc_error(const uint32_t status) {
	return (status & SDIO_RINTSTS_RCRC(1)) ? true : false;
}

static bool sdio_response_timed_out(const uint32_t status) {
	return (status & SDIO_RINTSTS_RTO_BAR(1)) ? true : false;
}

static sdio_error_t sdio_command_no_data(const uint32_t command, const uint32_t command_argument) {
	SDIO_RINTSTS = 0xffffffff;

	SDIO_CMDARG = command_argument;
	SDIO_CMD = command;
	sdio_wait_for_command_accepted();

	const uint32_t status_after_command_accepted = SDIO_RINTSTS;
	SDIO_RINTSTS = status_after_command_accepted;

	if( sdio_error_hardware_is_locked(status_after_command_accepted) ) {
		return SDIO_ERROR_HARDWARE_IS_LOCKED;
	}

	while( !sdio_command_is_complete(SDIO_RINTSTS) );
	SDIO_RINTSTS = SDIO_RINTSTS_CDONE(1);

	const uint32_t status_after_command_complete = SDIO_RINTSTS;
	SDIO_RINTSTS = status_after_command_complete;

	if( sdio_response_timed_out(status_after_command_complete) ) {
		return SDIO_ERROR_RESPONSE_TIMED_OUT;
	}

	if( sdio_response_crc_error(status_after_command_complete) ) {
		return SDIO_ERROR_RESPONSE_CRC_ERROR;
	}

	if( sdio_response_error(status_after_command_complete) ) {
		return SDIO_ERROR_RESPONSE_ERROR;
	}

	return SDIO_OK;
}

sdio_error_t sdio_cmd0(const uint_fast8_t init) {
	// Enter IDLE state
	uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD0_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(0)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(0)
		| SDIO_CMD_DATA_EXPECTED(0)
		| SDIO_CMD_READ_WRITE(0)
		| SDIO_CMD_TRANSFER_MODE(0)
		| SDIO_CMD_SEND_AUTO_STOP(0)
		| SDIO_CMD_WAIT_PRVDATA_COMPLETE(1)
		| SDIO_CMD_STOP_ABORT_CMD(0)
		| SDIO_CMD_SEND_INITIALIZATION(init)
		| SDIO_CMD_UPDATE_CLOCK_REGISTERS_ONLY(0)
		| SDIO_CMD_READ_CEATA_DEVICE(0)
		| SDIO_CMD_CCS_EXPECTED(0)
		| SDIO_CMD_ENABLE_BOOT(0)
		| SDIO_CMD_EXPECT_BOOT_ACK(0)
		| SDIO_CMD_DISABLE_BOOT(0)
		| SDIO_CMD_BOOT_MODE(0)
		| SDIO_CMD_VOLT_SWITCH(0)
		| SDIO_CMD_START_CMD(1)
		;
	return sdio_command_no_data(command, 0);
}

sdio_error_t sdio_cmd2() {
	const uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD2_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(0)
		| SDIO_CMD_DATA_EXPECTED(0)
		| SDIO_CMD_READ_WRITE(0)
		| SDIO_CMD_TRANSFER_MODE(0)
		| SDIO_CMD_SEND_AUTO_STOP(0)
		| SDIO_CMD_WAIT_PRVDATA_COMPLETE(1)
		| SDIO_CMD_STOP_ABORT_CMD(0)
		| SDIO_CMD_SEND_INITIALIZATION(0)
		| SDIO_CMD_UPDATE_CLOCK_REGISTERS_ONLY(0)
		| SDIO_CMD_READ_CEATA_DEVICE(0)
		| SDIO_CMD_CCS_EXPECTED(0)
		| SDIO_CMD_ENABLE_BOOT(0)
		| SDIO_CMD_EXPECT_BOOT_ACK(0)
		| SDIO_CMD_DISABLE_BOOT(0)
		| SDIO_CMD_BOOT_MODE(0)
		| SDIO_CMD_VOLT_SWITCH(0)
		| SDIO_CMD_START_CMD(1)
		;
	return sdio_command_no_data(command, 0);
}

sdio_error_t sdio_cmd3() {
	const uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD3_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(0)
		| SDIO_CMD_DATA_EXPECTED(0)
		| SDIO_CMD_READ_WRITE(0)
		| SDIO_CMD_TRANSFER_MODE(0)
		| SDIO_CMD_SEND_AUTO_STOP(0)
		| SDIO_CMD_WAIT_PRVDATA_COMPLETE(1)
		| SDIO_CMD_STOP_ABORT_CMD(0)
		| SDIO_CMD_SEND_INITIALIZATION(0)
		| SDIO_CMD_UPDATE_CLOCK_REGISTERS_ONLY(0)
		| SDIO_CMD_READ_CEATA_DEVICE(0)
		| SDIO_CMD_CCS_EXPECTED(0)
		| SDIO_CMD_ENABLE_BOOT(0)
		| SDIO_CMD_EXPECT_BOOT_ACK(0)
		| SDIO_CMD_DISABLE_BOOT(0)
		| SDIO_CMD_BOOT_MODE(0)
		| SDIO_CMD_VOLT_SWITCH(0)
		| SDIO_CMD_START_CMD(1)
		;
	return sdio_command_no_data(command, 0);
}

sdio_error_t sdio_cmd8() {
	const uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD8_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(0)
		| SDIO_CMD_DATA_EXPECTED(0)
		| SDIO_CMD_READ_WRITE(0)
		| SDIO_CMD_TRANSFER_MODE(0)
		| SDIO_CMD_SEND_AUTO_STOP(0)
		| SDIO_CMD_WAIT_PRVDATA_COMPLETE(1)
		| SDIO_CMD_STOP_ABORT_CMD(0)
		| SDIO_CMD_SEND_INITIALIZATION(0)
		| SDIO_CMD_UPDATE_CLOCK_REGISTERS_ONLY(0)
		| SDIO_CMD_READ_CEATA_DEVICE(0)
		| SDIO_CMD_CCS_EXPECTED(0)
		| SDIO_CMD_ENABLE_BOOT(0)
		| SDIO_CMD_EXPECT_BOOT_ACK(0)
		| SDIO_CMD_DISABLE_BOOT(0)
		| SDIO_CMD_BOOT_MODE(0)
		| SDIO_CMD_VOLT_SWITCH(0)
		| SDIO_CMD_START_CMD(1)
		;
	const uint32_t voltage_supplied = 0b0001;	// 2.7 - 3.6V
	const uint32_t check_pattern = 0b10101010;
	const uint32_t arg =
		  (voltage_supplied << 8)
		| (check_pattern << 0)
		;
	const int32_t result = sdio_command_no_data(command, arg);
	if( result != SDIO_OK ) {
		return result;
	}
	if( ((SDIO_RESP0 >> 0) & 0xff) != check_pattern ) {
		return SDIO_ERROR_RESPONSE_CHECK_PATTERN_INCORRECT;
	}
	if( ((SDIO_RESP0 >> 8) & 0xf) != voltage_supplied ) {
		return SDIO_ERROR_RESPONSE_VOLTAGE_NOT_ACCEPTED;
	}
	return SDIO_OK;
}

sdio_error_t sdio_cmd55() {
	const uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD55_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(0)
		| SDIO_CMD_DATA_EXPECTED(0)
		| SDIO_CMD_READ_WRITE(0)
		| SDIO_CMD_TRANSFER_MODE(0)
		| SDIO_CMD_SEND_AUTO_STOP(0)
		| SDIO_CMD_WAIT_PRVDATA_COMPLETE(1)
		| SDIO_CMD_STOP_ABORT_CMD(0)
		| SDIO_CMD_SEND_INITIALIZATION(0)
		| SDIO_CMD_UPDATE_CLOCK_REGISTERS_ONLY(0)
		| SDIO_CMD_READ_CEATA_DEVICE(0)
		| SDIO_CMD_CCS_EXPECTED(0)
		| SDIO_CMD_ENABLE_BOOT(0)
		| SDIO_CMD_EXPECT_BOOT_ACK(0)
		| SDIO_CMD_DISABLE_BOOT(0)
		| SDIO_CMD_BOOT_MODE(0)
		| SDIO_CMD_VOLT_SWITCH(0)
		| SDIO_CMD_START_CMD(1)
		;
	return sdio_command_no_data(command, 0);
}

sdio_error_t sdio_acmd41(const uint32_t vdd_voltage_window) {
	const int result_cmd55 = sdio_cmd55();
	if( result_cmd55 != SDIO_OK ) {
		return result_cmd55;
	}

	const uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_ACMD41_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(0)
		| SDIO_CMD_DATA_EXPECTED(0)
		| SDIO_CMD_READ_WRITE(0)
		| SDIO_CMD_TRANSFER_MODE(0)
		| SDIO_CMD_SEND_AUTO_STOP(0)
		| SDIO_CMD_WAIT_PRVDATA_COMPLETE(1)
		| SDIO_CMD_STOP_ABORT_CMD(0)
		| SDIO_CMD_SEND_INITIALIZATION(0)
		| SDIO_CMD_UPDATE_CLOCK_REGISTERS_ONLY(0)
		| SDIO_CMD_READ_CEATA_DEVICE(0)
		| SDIO_CMD_CCS_EXPECTED(0)
		| SDIO_CMD_ENABLE_BOOT(0)
		| SDIO_CMD_EXPECT_BOOT_ACK(0)
		| SDIO_CMD_DISABLE_BOOT(0)
		| SDIO_CMD_BOOT_MODE(0)
		| SDIO_CMD_VOLT_SWITCH(0)
		| SDIO_CMD_START_CMD(1)
		;
	const uint32_t hcs = 1;		// SDSC-only host
	const uint32_t xpc = 0;		// Power saving
	const uint32_t s18r = 0;	// Use current voltage
	const uint32_t arg =
		  (hcs << 30)
		| (xpc << 28)
		| (s18r << 24)
		| (vdd_voltage_window <<  0)
		;
	const int result = sdio_command_no_data(command, arg);
	if( result != SDIO_OK ) {
		return result;
	}
	if( ((SDIO_RESP0 >> 31) & 1) != 1 ) {
		return SDIO_ERROR_RESPONSE_ON_INITIALIZATION;
	}
	return SDIO_OK;
}

/* LPC43xx SDIO peripheral is the Synopsys DesignWare
 * Multimedia Card Interface driver
 * See Linux kernel: drivers/mmc/host/dw_mmc.c
 */
void sdio_init() {
	/* Switch SDIO clock over to use PLL1 (204MHz) */
	CGU_BASE_SDIO_CLK = CGU_BASE_SDIO_CLK_AUTOBLOCK(1)
			| CGU_BASE_SDIO_CLK_CLK_SEL(CGU_SRC_PLL1);

	sdio_controller_reset();

	SDIO_INTMASK = 0;
	SDIO_RINTSTS = 0xffffffff;
	SDIO_TMOUT = 0xffffffff;
	SDIO_CLKENA = 0;
	SDIO_CLKSRC = 0;

	sdio_clear_interrupts();
}
