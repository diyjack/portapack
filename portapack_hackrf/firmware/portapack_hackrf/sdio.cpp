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

#define SDIO_CMD7_INDEX (0b000111)

#define SDIO_CMD8_INDEX (0b001000)

#define SDIO_CMD17_INDEX (0b010001)

#define SDIO_CMD24_INDEX (0b011000)

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

static void sdio_cclk_set(const uint32_t divider_index) {
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
	SDIO_CLKSRC = SDIO_CLKSRC_CLK_SOURCE(divider_index);
	sdio_update_clock_registers_only();

	SDIO_CLKDIV =
		  SDIO_CLKDIV_CLK_DIVIDER0(0xff)
		| SDIO_CLKDIV_CLK_DIVIDER1(0x5)
		| SDIO_CLKDIV_CLK_DIVIDER2(0)
		| SDIO_CLKDIV_CLK_DIVIDER3(0)
		;
	sdio_update_clock_registers_only();

	SDIO_CLKENA |= SDIO_CLKENA_CCLK_ENABLE(1);
	sdio_update_clock_registers_only();
}

void sdio_cclk_set_400khz() {
	sdio_cclk_set(0);
}

void sdio_cclk_set_20mhz() {
	sdio_cclk_set(1);
}

void sdio_set_width_1bit() {
	SDIO_CTYPE =
		  SDIO_CTYPE_CARD_WIDTH0(0)
		| SDIO_CTYPE_CARD_WIDTH1(0)
		;
}

bool sdio_card_is_present() {
	return (SDIO_CDETECT & SDIO_CDETECT_CARD_DETECT_MASK) ? false : true;
}

static bool sdio_error_hardware_is_locked(const uint32_t status) {
	return (status & SDIO_RINTSTS_HLE_MASK) ? true : false;
}

static bool sdio_command_is_complete(const uint32_t status) {
	return (status & SDIO_RINTSTS_CDONE_MASK) ? true : false;
}

static sdio_error_t sdio_status(const uint32_t intsts) {
	/* TODO: This is terrible. Make a map or something. */
	/*
	const uint32_t errors_mask =
		  SDIO_RINTSTS_RE_MASK
		| SDIO_RINTSTS_RCRC_MASK
		| SDIO_RINTSTS_DCRC_MASK
		| SDIO_RINTSTS_RTO_BAR_MASK
		| SDIO_RINTSTS_DRTO_BDS_MASK
		| SDIO_RINTSTS_HTO_MASK
		| SDIO_RINTSTS_FRUN_MASK
		| SDIO_RINTSTS_HLE_MASK
		| SDIO_RINTSTS_SBE_MASK
		| SDIO_RINTSTS_EBE_MASK
		;
	*/
	if( intsts & SDIO_RINTSTS_RE_MASK ) {
		return SDIO_ERROR_RESPONSE_ERROR;
	}

	if( intsts & SDIO_RINTSTS_RCRC_MASK ) {
		return SDIO_ERROR_RESPONSE_CRC_ERROR;
	}

	if( intsts & SDIO_RINTSTS_DCRC_MASK ) {
		return SDIO_ERROR_DATA_CRC_ERROR;
	}

	if( intsts & SDIO_RINTSTS_RTO_BAR_MASK ) {
		return SDIO_ERROR_RESPONSE_TIMED_OUT;
	}

	if( intsts & SDIO_RINTSTS_DRTO_BDS_MASK ) {
		return SDIO_ERROR_DATA_READ_TIMED_OUT;
	}

	if( intsts & SDIO_RINTSTS_HTO_MASK ) {
		return SDIO_ERROR_DATA_STARVATION_BY_HOST_TIMEOUT;
	}

	if( intsts & SDIO_RINTSTS_FRUN_MASK ) {
		return SDIO_ERROR_FIFO_OVER_UNDERRUN_ERROR;
	}

	if( intsts & SDIO_RINTSTS_HLE_MASK ) {
		return SDIO_ERROR_HARDWARE_IS_LOCKED;
	}

	if( intsts & SDIO_RINTSTS_SBE_MASK ) {
		return SDIO_ERROR_START_BIT;
	}

	if( intsts & SDIO_RINTSTS_EBE_MASK ) {
		return SDIO_ERROR_END_BIT;
	}

	return SDIO_OK;
}

static sdio_error_t sdio_command_no_data(const uint32_t command, const uint32_t command_argument) {
	sdio_clear_interrupts();

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

	return sdio_status(SDIO_RINTSTS);
}

static const size_t sdio_sector_size = 512;
static const bool sdio_sdhc_or_sdxc = false;

sdio_error_t sdio_read(const uint32_t sector, uint32_t* buffer, const size_t sector_count) {
	sdio_clear_interrupts();

	SDIO_BYTCNT = SDIO_BYTCNT_BYTE_COUNT(sector_count * sdio_sector_size);
	SDIO_BLKSIZ = SDIO_BLKSIZ_BLOCK_SIZE(sdio_sector_size);

	SDIO_CMDARG = sdio_sdhc_or_sdxc ? sector : (sector * sdio_sector_size);
	SDIO_CMD =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD17_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(1)
		| SDIO_CMD_DATA_EXPECTED(1)
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
	sdio_wait_for_command_accepted();

	while( !sdio_command_is_complete(SDIO_RINTSTS) );
	SDIO_RINTSTS = SDIO_RINTSTS_CDONE(1);

	sdio_error_t status = sdio_status(SDIO_RINTSTS);
	if( status != SDIO_OK ) {
		return status;
	}

	const uint32_t data_transfer_over_mask =
		  SDIO_RINTSTS_DTO_MASK
		| SDIO_RINTSTS_DCRC_MASK
		| SDIO_RINTSTS_DRTO_BDS_MASK
		| SDIO_RINTSTS_HTO_MASK
		| SDIO_RINTSTS_SBE_MASK
		| SDIO_RINTSTS_EBE_MASK
		;
	while( (SDIO_RINTSTS & data_transfer_over_mask) == 0 ) {
		while( (SDIO_STATUS & SDIO_STATUS_FIFO_EMPTY_MASK) == 0 ) {
			*(buffer++) = SDIO_DATA;
		}
	}

	while( (SDIO_STATUS & SDIO_STATUS_FIFO_EMPTY_MASK) == 0 ) {
		*(buffer++) = SDIO_DATA;
	}

	return sdio_status(SDIO_RINTSTS);
}

sdio_error_t sdio_write(const uint32_t sector, const uint32_t* buffer, const size_t sector_count) {
	sdio_clear_interrupts();

	const uint32_t bytes_to_send = sector_count * sdio_sector_size;
	SDIO_BYTCNT = SDIO_BYTCNT_BYTE_COUNT(bytes_to_send);
	uint32_t words_to_send = bytes_to_send / sizeof(uint32_t);
	SDIO_BLKSIZ = SDIO_BLKSIZ_BLOCK_SIZE(sdio_sector_size);

	SDIO_CMDARG = sdio_sdhc_or_sdxc ? sector : (sector * sdio_sector_size);

	while( (words_to_send > 0) && ((SDIO_STATUS & SDIO_STATUS_FIFO_FULL_MASK) == 0) ) {
		SDIO_DATA = *(buffer++);
		words_to_send--;
	}

	SDIO_CMD =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD24_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(1)
		| SDIO_CMD_DATA_EXPECTED(1)
		| SDIO_CMD_READ_WRITE(1)
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
	sdio_wait_for_command_accepted();

	while( !sdio_command_is_complete(SDIO_RINTSTS) );
	SDIO_RINTSTS = SDIO_RINTSTS_CDONE(1);

	sdio_error_t status = sdio_status(SDIO_RINTSTS);
	if( status != SDIO_OK ) {
		return status;
	}

	const uint32_t data_transfer_over_mask =
		  SDIO_RINTSTS_DTO_MASK
		| SDIO_RINTSTS_DCRC_MASK
		| SDIO_RINTSTS_DRTO_BDS_MASK
		| SDIO_RINTSTS_HTO_MASK
		| SDIO_RINTSTS_SBE_MASK
		| SDIO_RINTSTS_EBE_MASK
		;
	while( (SDIO_RINTSTS & data_transfer_over_mask) == 0 ) {
		while( (words_to_send > 0) && ((SDIO_STATUS & SDIO_STATUS_FIFO_FULL_MASK) == 0) ) {
			SDIO_DATA = *(buffer++);
			words_to_send--;
		}
	}

	while( SDIO_STATUS & SDIO_STATUS_DATA_BUSY_MASK ) {
		// Wait for card not busy.
	}

	return sdio_status(SDIO_RINTSTS);
}

sdio_error_t sdio_cmd0(const uint_fast8_t init) {
	// Enter IDLE state
	uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD0_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(0)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(1)
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
		| SDIO_CMD_RESPONSE_LENGTH(1)
		| SDIO_CMD_CHECK_RESPONSE_CRC(1)
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
		| SDIO_CMD_CHECK_RESPONSE_CRC(1)
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

sdio_error_t sdio_cmd7(const uint32_t rca) {
	const uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD7_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(1)
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
	const uint32_t arg =
		  (rca << 16)
		| (  0 <<  0)
		;
	return sdio_command_no_data(command, arg);
}

sdio_error_t sdio_cmd8() {
	const uint32_t command =
		  SDIO_CMD_CMD_INDEX(SDIO_CMD8_INDEX)
		| SDIO_CMD_RESPONSE_EXPECT(1)
		| SDIO_CMD_RESPONSE_LENGTH(0)
		| SDIO_CMD_CHECK_RESPONSE_CRC(1)
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
	const sdio_error_t result = sdio_command_no_data(command, arg);
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
		| SDIO_CMD_CHECK_RESPONSE_CRC(1)
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
	const uint32_t rca = 0;
	const uint32_t arg =
		  (rca << 16)
		| (  0 <<  0)
		;
	return sdio_command_no_data(command, arg);
}

sdio_error_t sdio_acmd41(const uint32_t hcs) {
	const sdio_error_t result_cmd55 = sdio_cmd55();
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
	const uint32_t xpc = 0;		// Power saving
	const uint32_t s18r = 0;	// Use current voltage
	const uint32_t vdd_voltage_window = 0xff8000;
	const uint32_t arg =
		  (hcs << 30)
		| (xpc << 28)
		| (s18r << 24)
		| (vdd_voltage_window << 0)
		;
	const sdio_error_t result = sdio_command_no_data(command, arg);
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
	SDIO_CLKENA = 0;
	sdio_clear_interrupts();
	SDIO_TMOUT = 0xffffff40;
	SDIO_CLKSRC = 0;

	sdio_clear_interrupts();
}
