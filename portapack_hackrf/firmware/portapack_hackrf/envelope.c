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

#include "envelope.h"

void envelope_init(
	envelope_t* const envelope,
	const float rise_factor,
	const float fall_factor
) {
	envelope->rise_factor = rise_factor;
	envelope->fall_factor = fall_factor;
	envelope->envelope_minimum = 0.000001f;
	envelope->envelope = envelope->envelope_minimum;
}

float envelope_execute(
	envelope_t* const envelope,
	const float magnitude
) {
	if( magnitude > envelope->envelope ) {
		envelope->envelope = envelope->envelope * (1.0f - envelope->rise_factor) + magnitude * envelope->rise_factor;
	} else {
		envelope->envelope = envelope->envelope * (1.0f - envelope->fall_factor) + magnitude * envelope->fall_factor;
	}
	const float out = (2.0f * magnitude / (envelope->envelope + envelope->envelope_minimum)) - 1.0f;
	return out;
}
