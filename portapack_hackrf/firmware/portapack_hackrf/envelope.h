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

#ifndef __ENVELOPE_H__
#define __ENVELOPE_H__

typedef struct envelope_t {
	float rise_factor;
	float fall_factor;
	float envelope;
} envelope_t;

void envelope_init(
	envelope_t* const envelope,
	const float rise_factor,
	const float fall_factor
);

float envelope_execute(
	envelope_t* const envelope,
	const float magnitude
);

#endif/*__ENVELOPE_H__*/
