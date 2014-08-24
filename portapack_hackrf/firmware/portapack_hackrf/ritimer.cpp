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

#include "ritimer.h"

#include <libopencm3/lpc43xx/ritimer.h>
#include <libopencm3/cm3/nvic.h>

void ritimer_init() {
	RITIMER_CTRL =
		  RITIMER_CTRL_RITINT(1)
		| RITIMER_CTRL_RITENCLR(0)
		| RITIMER_CTRL_RITENBR(1)
		| RITIMER_CTRL_RITEN(0)
		;
	RITIMER_COMPVAL = 0xffffffff;
	RITIMER_MASK = 0;
	RITIMER_COUNTER = 0;
}

void ritimer_compare_set(const uint32_t value) {
	RITIMER_COMPVAL = value;
}

void ritimer_match_clear_enable() {
	RITIMER_CTRL |= RITIMER_CTRL_RITENCLR(1);
}

void ritimer_enable() {
	RITIMER_CTRL |= RITIMER_CTRL_RITEN(1);
}

void ritimer_interrupt_clear() {
	RITIMER_CTRL |= (1 << 0);
}
