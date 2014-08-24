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

#include "clock_recovery.h"

#include <stdbool.h>

#include "math.h"

void clock_recovery_init(
	clock_recovery_t* const clock_recovery,
	const float fractional_symbol_rate,
	clock_recovery_symbol_handler_t symbol_handler,
	void* const context
) {
	clock_recovery->phase = 0;
	clock_recovery->phase_last = 0;
	clock_recovery->phase_adjustment = 0;
	clock_recovery->phase_increment = (uint32_t)roundf((1ULL << 32) * fractional_symbol_rate);
	clock_recovery->t0 = 0;
	clock_recovery->t1 = 0;
	clock_recovery->t2 = 0;
	clock_recovery->error_filtered = 0;
	clock_recovery->symbol_handler = symbol_handler;
	clock_recovery->context = context;
}

void clock_recovery_execute(
	clock_recovery_t* const clock_recovery,
	const float in
) {
	const bool phase_0 = (clock_recovery->phase_last >> 31) & (!(clock_recovery->phase >> 31));
	const bool phase_180 = (!(clock_recovery->phase_last >> 31)) & (clock_recovery->phase >> 31);
	clock_recovery->phase_last = clock_recovery->phase;
	clock_recovery->phase += clock_recovery->phase_increment + clock_recovery->phase_adjustment;

	if( phase_0 || phase_180 ) {
		clock_recovery->t2 = clock_recovery->t1;
		clock_recovery->t1 = clock_recovery->t0;
		clock_recovery->t0 = in;
	}

	if( phase_0 ) {
		clock_recovery->symbol_handler(clock_recovery->t0, clock_recovery->context);

		const float error = (clock_recovery->t0 - clock_recovery->t2) * clock_recovery->t1;
		// + error == late == decrease/slow phase
		// - error == early == increase/fast phase

		clock_recovery->error_filtered = 0.75f * clock_recovery->error_filtered + 0.25f * error;

		// Correct phase (don't change frequency!)
		clock_recovery->phase_adjustment = -clock_recovery->phase_increment * clock_recovery->error_filtered / 200.0f;
	}
}
