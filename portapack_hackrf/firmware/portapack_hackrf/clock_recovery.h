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

#ifndef __CLOCK_RECOVERY_H__
#define __CLOCK_RECOVERY_H__

#include <stdint.h>

typedef void (*clock_recovery_symbol_handler_t)(const float value, void* const context);

typedef struct clock_recovery_t {
	uint32_t phase;
	uint32_t phase_last;
	uint32_t phase_adjustment;
	uint32_t phase_increment;
	float t0, t1, t2;
	float error_filtered;
	clock_recovery_symbol_handler_t symbol_handler;
	void* context;
} clock_recovery_t;

void clock_recovery_init(
	clock_recovery_t* const clock_recovery,
	const float fractional_symbol_rate,
	clock_recovery_symbol_handler_t symbol_handler,
	void* const context
);

void clock_recovery_execute(
	clock_recovery_t* const clock_recovery,
	const float in
);

#endif/*__CLOCK_RECOVERY_H__*/
