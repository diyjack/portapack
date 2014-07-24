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

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <stdint.h>

#include "lcd.h"

typedef struct console_t {
	lcd_t* lcd;
	uint16_t x;
	uint16_t y;
} console_t;

void console_init(console_t* const console, lcd_t* const lcd, const uint_fast16_t y_top, const uint_fast16_t height);

void console_write(console_t* const console, const char* message);
void console_writeln(console_t* const console, const char* message);
void console_write_uint32(console_t* const console, const char* format, const uint32_t value);

#endif/*__CONSOLE_H__*/
