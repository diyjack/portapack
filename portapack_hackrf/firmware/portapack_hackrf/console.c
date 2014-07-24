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

#include "console.h"

#include <stdint.h>
#include <stddef.h>

#include <stdio.h>

#include "lcd.h"

void console_init(console_t* const console, lcd_t* const lcd, const uint_fast16_t y_top, const uint_fast16_t height) {
	console->lcd = lcd;
	console->background = color_black;
	console->foreground = color_white;
	console->x = 0;
	console->y = 0;
	lcd_set_scroll_area(lcd, y_top, y_top + height);

	const lcd_color_t prior_background = lcd_set_background(console->lcd, console->background);
	lcd_clear_rectangle(console->lcd, 0, y_top, lcd->size.w, height);
	lcd_set_background(console->lcd, prior_background);
}

static void console_crlf(console_t* const console) {
	const uint_fast16_t line_height = console->lcd->font->line_height;
	console->x = 0;
	console->y += line_height;
	const int32_t y_excess = console->y + console->lcd->font->line_height - console->lcd->scroll.height;
	if( y_excess > 0 ) {
		lcd_scroll(console->lcd, -line_height);
		console->y -= y_excess;

		const lcd_color_t prior_background = lcd_set_background(console->lcd, console->background);
		lcd_clear_rectangle(console->lcd, 0, lcd_scroll_area_y(console->lcd, console->y), console->lcd->size.w, line_height);
		lcd_set_background(console->lcd, prior_background);
	}
}

void console_write(console_t* const console, const char* message) {
	char c;
	
	const lcd_color_t prior_background = lcd_set_background(console->lcd, console->background);
	const lcd_color_t prior_foreground = lcd_set_foreground(console->lcd, console->foreground);

	while( (c = *(message++)) != 0 ) {
		const lcd_glyph_t* const glyph = lcd_get_glyph(console->lcd, c);
		if( (console->x + glyph->advance) > console->lcd->size.w ) {
			console_crlf(console);
		}
		lcd_draw_char(
			console->lcd,
			console->x,
			lcd_scroll_area_y(console->lcd, console->y),
			c
		);
		console->x += glyph->advance;
	}

	lcd_set_background(console->lcd, prior_background);
	lcd_set_foreground(console->lcd, prior_foreground);
}

void console_writeln(console_t* const console, const char* message) {
	console_write(console, message);
	console_crlf(console);
}

void console_write_uint32(console_t* const console, const char* format, const uint32_t value) {
	char temp[80];
	const size_t temp_len = 79;
	snprintf(temp, temp_len, format, value);
	console_write(console, temp);
}
