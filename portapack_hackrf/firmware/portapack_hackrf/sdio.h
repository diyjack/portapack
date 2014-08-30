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
#include <stdbool.h>

typedef enum {
	SDIO_OK = 0,

	SDIO_ERROR_HARDWARE_IS_LOCKED = -1,
	SDIO_ERROR_RESPONSE_TIMED_OUT = -2,
	SDIO_ERROR_RESPONSE_CRC_ERROR = -3,
	SDIO_ERROR_RESPONSE_ERROR = -4,

	SDIO_ERROR_RESPONSE_ON_INITIALIZATION = -100,
	SDIO_ERROR_RESPONSE_CHECK_PATTERN_INCORRECT = -101,
	SDIO_ERROR_RESPONSE_VOLTAGE_NOT_ACCEPTED = -102,
} sdio_error_t;

void sdio_init();
bool sdio_card_is_present();

void sdio_cclk_set_400khz();
void sdio_cclk_set_20mhz();
void sdio_set_width_1bit();

sdio_error_t sdio_cmd0(const uint_fast8_t init);
sdio_error_t sdio_cmd8();
sdio_error_t sdio_acmd41(const uint32_t hcs);

#endif/*__SDIO_H__*/
