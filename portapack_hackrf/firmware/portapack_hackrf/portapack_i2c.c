/*
 * Copyright (C) 2013 Jared Boone, ShareBrained Technology, Inc.
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

#include "portapack_i2c.h"

#include <stdint.h>

#include <libopencm3/lpc43xx/i2c.h>

#include "wm8731.h"

void portapack_i2c_init() {
}

void portapack_audio_codec_write(const uint_fast8_t address, const uint_fast16_t data) {
	uint16_t word = (address << 9) | data;
	i2c0_tx_start();
	i2c0_tx_byte(WM8731_I2C_ADDR | I2C_WRITE);
	i2c0_tx_byte(word >> 8);
	i2c0_tx_byte(word & 0xff);
	i2c0_stop();
}
