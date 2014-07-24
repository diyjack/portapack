/*
 * Copyright (C) 2013 Jared Boone, ShareBrained Technology, Inc.
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

#ifndef __LCD_H__
#define __LCD_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint16_t x;
	uint16_t y;
} lcd_position_t;

typedef struct {
	uint16_t w;
	uint16_t h;
} lcd_size_t;

typedef uint16_t lcd_color_t;

#define LCD_COLOR(__r,__g,__b) ((((__r) >> 3) << 11) | (((__g) >> 2) << 5) | (((__b) >> 3) << 0))

extern const lcd_color_t color_black;
extern const lcd_color_t color_blue;
extern const lcd_color_t color_white;

typedef struct lcd_glyph_t {
	const uint8_t* const row;
	const uint8_t width;
	const uint8_t height;
	const uint8_t advance;
} lcd_glyph_t;

typedef struct lcd_font_t {
	const uint8_t char_advance;
	const uint8_t char_height;
	const uint8_t line_height;
	char glyph_table_start;
	char glyph_table_end;
	const lcd_glyph_t* glyph_table;
} lcd_font_t;

typedef struct lcd_scroll_t {
	uint16_t top_area;
	uint16_t bottom_area;
	uint16_t height;
	uint16_t current_position;
} lcd_scroll_t;

typedef struct lcd_colors_t {
	lcd_color_t background;
	lcd_color_t foreground;
} lcd_colors_t;

typedef struct lcd_t {
	lcd_size_t size;
	lcd_scroll_t scroll;
	lcd_colors_t colors;
	const lcd_font_t* font;
} lcd_t;

void lcd_init(lcd_t* const lcd);

void lcd_touch_sense_off();
void lcd_touch_sense_pressure();
void lcd_touch_sense_x(const bool invert);
void lcd_touch_sense_y(const bool invert);

void lcd_reset(lcd_t* const lcd);
void lcd_clear(const lcd_t* const lcd);

void lcd_data_write_rgb(const lcd_color_t color);

void lcd_start_drawing(
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h
);

void lcd_vertical_scrolling_definition(
	const uint_fast16_t top_fixed_area,
	const uint_fast16_t vertical_scrolling_area,
	const uint_fast16_t bottom_fixed_area
);

void lcd_vertical_scrolling_start_address(
	const uint_fast16_t vertical_scrolling_pointer
);

//uint_fast16_t lcd_get_scanline();
void lcd_frame_sync();

void lcd_set_font(lcd_t* const lcd, const lcd_font_t* const font);
lcd_color_t lcd_set_foreground(lcd_t* const lcd, const lcd_color_t color);
lcd_color_t lcd_set_background(lcd_t* const lcd, const lcd_color_t color);
void lcd_colors_invert(lcd_t* const lcd);

void lcd_fill_rectangle(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h
);

void lcd_draw_filled_rectangle(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h
);

void lcd_clear_rectangle(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h
);

void lcd_set_pixel(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y
);

const lcd_glyph_t* lcd_get_glyph(
	const lcd_t* const lcd,
	const char c
);

void lcd_draw_char(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const char c
);

void lcd_draw_string(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const char* const p,
	const uint_fast16_t len
);

void lcd_set_scroll_area(
	lcd_t* const lcd,
	const uint_fast16_t top_y,
	const uint_fast16_t bottom_y
);

uint_fast16_t lcd_scroll_area_y(
	lcd_t* const lcd,
	const uint_fast16_t y
);

uint_fast16_t lcd_scroll(
	lcd_t* const lcd,
	const int16_t delta
);

uint32_t lcd_data_read_switches();

#endif
