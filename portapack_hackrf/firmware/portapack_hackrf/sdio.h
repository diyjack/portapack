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

#ifndef __SDIO_H__
#define __SDIO_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
	SDIO_OK = 0,

	SDIO_ERROR_HARDWARE_IS_LOCKED = -1,
	SDIO_ERROR_RESPONSE_TIMED_OUT = -2,
	SDIO_ERROR_RESPONSE_CRC_ERROR = -3,
	SDIO_ERROR_RESPONSE_ERROR = -4,
	SDIO_ERROR_DATA_CRC_ERROR = -5,
	SDIO_ERROR_DATA_READ_TIMED_OUT = -6,
	SDIO_ERROR_DATA_STARVATION_BY_HOST_TIMEOUT = -7,
	SDIO_ERROR_FIFO_OVER_UNDERRUN_ERROR = -8,
	SDIO_ERROR_START_BIT = -9,
	SDIO_ERROR_END_BIT = -10,

	SDIO_ERROR_RESPONSE_ON_INITIALIZATION = -100,
	SDIO_ERROR_RESPONSE_CHECK_PATTERN_INCORRECT = -101,
	SDIO_ERROR_RESPONSE_VOLTAGE_NOT_ACCEPTED = -102,
} sdio_error_t;

void sdio_init();
bool sdio_card_is_present();

void sdio_cclk_set_400khz();
void sdio_cclk_set_20mhz();
void sdio_set_width_1bit();

sdio_error_t sdio_read(const uint32_t sector, uint32_t* buffer, const size_t sector_count);
sdio_error_t sdio_write(const uint32_t sector, const uint32_t* buffer, const size_t sector_count);

sdio_error_t sdio_cmd0(const uint_fast8_t init);
sdio_error_t sdio_cmd2();
sdio_error_t sdio_cmd3();
sdio_error_t sdio_cmd7(const uint32_t rca);
sdio_error_t sdio_cmd8();
sdio_error_t sdio_acmd41(const uint32_t hcs);

#endif/*__SDIO_H__*/
