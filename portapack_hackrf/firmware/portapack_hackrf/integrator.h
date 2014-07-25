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

#ifndef __INTEGRATOR_H__
#define __INTEGRATOR_H__

#include "stddef.h"

#include "complex.h"

typedef struct integrator_t {
	complex_s16_t delay[32];
	complex_s32_t sum;
	size_t length;
	size_t index;
} integrator_t;

void integrator_init(
	integrator_t* const integrator,
	const size_t length
);

complex_s16_t integrator_execute(
	integrator_t* const integrator,
	const complex_s16_t in
);

#endif/*__INTEGRATOR_H__*/
