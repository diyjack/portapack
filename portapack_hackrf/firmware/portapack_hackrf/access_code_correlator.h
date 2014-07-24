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

#ifndef __ACCESS_CODE_CORRELATOR_H__
#define __ACCESS_CODE_CORRELATOR_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct access_code_correlator_t {
	uint64_t code;
	uint64_t mask;
	uint64_t history;
	size_t maximum_hamming_distance;
} access_code_correlator_t;

void access_code_correlator_init(
	access_code_correlator_t* const correlator,
	const uint64_t code,
	const size_t code_length,
	const size_t maximum_hamming_distance
);

bool access_code_correlator_execute(
	access_code_correlator_t* const correlator,
	const uint_fast8_t in
);

#endif/*__ACCESS_CODE_CORRELATOR_H__*/
