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

#include "manchester.h"

#include <stdint.h>

void manchester_decode(
	const uint8_t* const in,
	uint8_t* const out,
	uint8_t* const errors,
	const size_t out_bits
) {
	for(size_t out_bit_n=0; out_bit_n<out_bits; out_bit_n++) {
		const size_t in_word_index = out_bit_n >> 2;
		const size_t in_bit_index = ((out_bit_n & 3) ^ 3) << 1;
		const uint_fast8_t in_symbol = (in[in_word_index] >> in_bit_index) & 3;

		const uint_fast8_t out_symbol = in_symbol & 1;
		const uint_fast8_t out_error = (in_symbol == 0b00) | (in_symbol == 0b11);
		const size_t out_word_index = out_bit_n >> 3;
		const size_t out_bit_index = (out_bit_n & 7) ^ 7;
		const uint8_t out_bit = (1 << out_bit_index);
		
		if( out_symbol ) {
			out[out_word_index] |= out_bit;
		} else {
			out[out_word_index] &= ~out_bit;
		}
		
		if( out_error ) {
			errors[out_word_index] |= out_bit;
		} else {
			errors[out_word_index] &= ~out_bit;
		}
	}
}
