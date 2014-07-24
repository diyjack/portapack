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

#include "integrator.h"

#include "linux_stuff.h"

void integrator_init(
	integrator_t* const integrator,
	const size_t length
) {
	for(size_t i=0; i<ARRAY_SIZE(integrator->delay); i++) {
		integrator->delay[i].i = 0;
		integrator->delay[i].q = 0;
	}
	integrator->sum.i = 0;
	integrator->sum.q = 0;
	integrator->length = length;
	integrator->index = 0;
}

complex_s16_t integrator_execute(
	integrator_t* const integrator,
	const complex_s16_t in
) {
	integrator->sum.i -= integrator->delay[integrator->index].i;
	integrator->sum.q -= integrator->delay[integrator->index].q;

	integrator->delay[integrator->index] = in;
	
	integrator->sum.i += in.i;
	integrator->sum.q += in.q;
	
	complex_s16_t out = { integrator->sum.i / 32, integrator->sum.q / 32 };

	integrator->index += 1;
	if( integrator->index >= integrator->length ) {
		integrator->index = 0;
	}

	return out;
}
