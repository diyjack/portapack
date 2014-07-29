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

#include <hackrf_core.h>

#include "portapack_driver.h"
#include "lcd.h"

static void delay_ms(uint32_t ms) {
	while((ms--) > 0) {
		delay(20000);
	}
}

void lcd_reset(lcd_t* const lcd) {
	(void)lcd;
	
	portapack_lcd_reset(false);
	delay_ms(1);
	portapack_lcd_reset(true);
	delay_ms(10);
	portapack_lcd_reset(false);
	delay_ms(120);
}

static void lcd_data_write_command(const uint_fast8_t value) {
	portapack_lcd_write(0, value);
}

static void lcd_data_write_data(const uint_fast8_t value) {
	portapack_lcd_write(1, value);
}

void lcd_data_write_rgb(const lcd_color_t color) {
	portapack_lcd_write_data_fast(color);
}

static void lcd_caset(const uint_fast16_t start_column, uint_fast16_t end_column) {
	lcd_data_write_command(0x2a);
	lcd_data_write_data(start_column >> 8);
	lcd_data_write_data(start_column & 0xff);
	lcd_data_write_data(end_column >> 8);
	lcd_data_write_data(end_column & 0xff);
}

static void lcd_paset(const uint_fast16_t start_page, const uint_fast16_t end_page) {
	lcd_data_write_command(0x2b);
	lcd_data_write_data(start_page >> 8);
	lcd_data_write_data(start_page & 0xff);
	lcd_data_write_data(end_page >> 8);
	lcd_data_write_data(end_page & 0xff);
}

static void lcd_ramwr_start() {
	lcd_data_write_command(0x2c);
}

void lcd_start_drawing(
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h
) {
	lcd_caset(x, x + w - 1);
	lcd_paset(y, y + h - 1);
	lcd_ramwr_start();
}

void lcd_vertical_scrolling_definition(
	const uint_fast16_t top_fixed_area,
	const uint_fast16_t vertical_scrolling_area,
	const uint_fast16_t bottom_fixed_area
) {
	lcd_data_write_command(0x33);
	lcd_data_write_data(top_fixed_area >> 8);
	lcd_data_write_data(top_fixed_area & 0xff);
	lcd_data_write_data(vertical_scrolling_area >> 8);
	lcd_data_write_data(vertical_scrolling_area & 0xff);
	lcd_data_write_data(bottom_fixed_area >> 8);
	lcd_data_write_data(bottom_fixed_area & 0xff);
}

void lcd_vertical_scrolling_start_address(
	const uint_fast16_t vertical_scrolling_pointer
) {
	lcd_data_write_command(0x37);
	lcd_data_write_data(vertical_scrolling_pointer >> 8);
	lcd_data_write_data(vertical_scrolling_pointer & 0xff);
}
/*
uint_fast16_t lcd_get_scanline() {
	lcd_data_write_command(0x45);
	lcd_data_read();
	const uint_fast8_t gts_h = lcd_data_read() & 0x3;
	const uint_fast8_t gts_l = lcd_data_read() & 0xff;
	return (gts_h << 8) | gts_l;
}
*/
void lcd_frame_sync() {
	portapack_lcd_frame_sync();
}

const lcd_color_t color_black = LCD_COLOR(0, 0, 0);
const lcd_color_t color_blue  = LCD_COLOR(0, 0, 255);
const lcd_color_t color_red   = LCD_COLOR(255, 0, 0);
const lcd_color_t color_white = LCD_COLOR(255, 255, 255);

const lcd_glyph_t* lcd_get_glyph(const lcd_t* const lcd, const char c) {
	const lcd_font_t* const font = lcd->font;
	if( (c >= font->glyph_table_start) && (c <= font->glyph_table_end) ) {
		return &font->glyph_table[c - font->glyph_table_start];
	} else {
		return &font->glyph_table[0];
	}
}

static uint_fast16_t lcd_string_width(
	const lcd_t* const lcd,
	const char* const s,
	const uint_fast16_t len
) {
	uint_fast16_t width = 0;
	for(uint_fast16_t i=0; i<len; i++) {
		width += lcd_get_glyph(lcd, s[i])->advance;
	}
	return width;
}

void lcd_set_font(lcd_t* const lcd, const lcd_font_t* const font) {
	lcd->font = font;
}

lcd_color_t lcd_set_foreground(lcd_t* const lcd, const lcd_color_t color) {
	const lcd_color_t prior_color = lcd->colors.foreground;
	lcd->colors.foreground = color;
	return prior_color;
}

lcd_color_t lcd_set_background(lcd_t* const lcd, const lcd_color_t color) {
	const lcd_color_t prior_color = lcd->colors.background;
	lcd->colors.background = color;
	return prior_color;
}

void lcd_colors_invert(
	lcd_t* const lcd
) {
	const lcd_color_t temp = lcd->colors.background;
	lcd->colors.background = lcd->colors.foreground;
	lcd->colors.foreground = temp;
}

static void lcd_fill_rectangle_with_color(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h,
	const lcd_color_t color
) {
	(void)lcd;

	lcd_caset(x, x + w - 1);
	lcd_paset(y, y + h - 1);
	lcd_ramwr_start();

	for(uint_fast16_t ty=0; ty<h; ty++) {
		for(uint_fast16_t tx=0; tx<w; tx++) {
			lcd_data_write_rgb(color);
		}
	}
}

void lcd_fill_rectangle(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h
) {
	lcd_fill_rectangle_with_color(lcd, x, y, w, h, lcd->colors.foreground);
}

void lcd_draw_filled_rectangle(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h
) {
	lcd_fill_rectangle_with_color(lcd, x,     y,     w, 1, lcd->colors.foreground);
	lcd_fill_rectangle_with_color(lcd, x,     y+h-1, w, 1, lcd->colors.foreground);
	lcd_fill_rectangle_with_color(lcd, x,     y,     1, h, lcd->colors.foreground);
	lcd_fill_rectangle_with_color(lcd, x+w-1, y,     1, h, lcd->colors.foreground);
	if( (w > 2) && (h > 2) ) {
		lcd_fill_rectangle_with_color(lcd, x + 1, y + 1, w - 2, h - 2, lcd->colors.background);
	}
}

void lcd_clear_rectangle(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const uint_fast16_t w,
	const uint_fast16_t h
) {
	lcd_fill_rectangle_with_color(lcd, x, y, w, h, lcd->colors.background);
}

void lcd_set_pixel(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y
) {
	lcd_fill_rectangle(lcd, x, y, 1, 1);
}

void lcd_draw_string(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const char* const p,
	const uint_fast16_t len
) {
	const lcd_font_t* const font = lcd->font;
	const uint_fast16_t string_width = lcd_string_width(lcd, p, len);

	lcd_caset(x, x + string_width - 1);
	lcd_paset(y, y + font->char_height - 1);
	lcd_ramwr_start();

	for(uint_fast16_t y=0; y<font->char_height; y++) {
		for(uint_fast16_t n=0; n<len; n++) {
			const char c = p[n];
			const lcd_glyph_t* const glyph = lcd_get_glyph(lcd, c);
			uint32_t row_data = glyph->row[y];
			for(uint_fast16_t x=glyph->advance; x>0; x--) {
				if( (row_data >> (x-1)) & 1 ) {
					lcd_data_write_rgb(lcd->colors.foreground);
				} else {
					lcd_data_write_rgb(lcd->colors.background);
				}
			}
		}
	}
}

void lcd_draw_char(
	const lcd_t* const lcd,
	const uint_fast16_t x,
	const uint_fast16_t y,
	const char c
) {
	/* TODO: Refactor to be a dependency of lcd_draw_string! */
	lcd_draw_string(lcd, x, y, &c, 1);
}
/*
static void lcd_command<size_t N> lcd_command(uint8_t command, const std::array<N>& data) {
	lcd_data_write_command(command);
	for(size_t i=0; i<N; i++) {
		lcd_data_write_data(data[i]);
	}
}
*/
static void portapack_init_lcd_kingtech() {
	lcd_data_write_command(0xCF);	// Power control B
	lcd_data_write_data(0x00);		// 0
	lcd_data_write_data(0xD9);		// PCEQ=1, DRV_ena=0, Power control=3
	lcd_data_write_data(0X30);

	lcd_data_write_command(0xED);	// Power on sequence control
	lcd_data_write_data(0x64);
	lcd_data_write_data(0x03);
	lcd_data_write_data(0X12);
	lcd_data_write_data(0X81);

	lcd_data_write_command(0xE8);	// Driver timing control A
	lcd_data_write_data(0x85);
	lcd_data_write_data(0x10);
	lcd_data_write_data(0x78);

	lcd_data_write_command(0xCB);	// Power control A
	lcd_data_write_data(0x39);
	lcd_data_write_data(0x2C);
	lcd_data_write_data(0x00);
	lcd_data_write_data(0x34);
	lcd_data_write_data(0x02);

	lcd_data_write_command(0xF7);	// Pump ratio control
	lcd_data_write_data(0x20);

	lcd_data_write_command(0xEA);	// Driver timing control B
	lcd_data_write_data(0x00);
	lcd_data_write_data(0x00);

	lcd_data_write_command(0xC0);	// Power Control 1
	lcd_data_write_data(0x1B);		//VRH[5:0] 

	lcd_data_write_command(0xC1);	// Power Control 2
	lcd_data_write_data(0x12);		//SAP[2:0];BT[3:0] 

	lcd_data_write_command(0xC5);	// VCOM Control 1
	lcd_data_write_data(0x32);
	lcd_data_write_data(0x3C);

	lcd_data_write_command(0xC7);	// VCOM Control 2
	lcd_data_write_data(0X9B);

	// Invert X and Y memory access order, so upper-left of
	// screen is (0,0) when writing to display.
	lcd_data_write_command(0x36);	// Memory Access Control 
	lcd_data_write_data(
		(1 << 7) |	// MY=1
		(1 << 6) |	// MX=1
		(0 << 5) |	// MV=0
		(1 << 4) |	// ML=1: reverse vertical refresh to simplify scrolling logic
		(1 << 3)	// BGR=1: For Kingtech LCD, BGR filter.
	);

	lcd_data_write_command(0x3A);	// COLMOD: Pixel Format Set
	lcd_data_write_data(0x55);		// DPI=16 bits/pixel, DBI=16 bits/pixel

	lcd_data_write_command(0xB1);
	lcd_data_write_data(0x00);
	lcd_data_write_data(0x1B);

	lcd_data_write_command(0xB6);	// Display Function Control 
	lcd_data_write_data(0x0A);
	lcd_data_write_data(0xA2);

	lcd_data_write_command(0xF6);
	lcd_data_write_data(0x01);
	lcd_data_write_data(0x30);

	lcd_data_write_command(0xF2);	// 3Gamma Function Disable 
	lcd_data_write_data(0x00);

	lcd_data_write_command(0x26);	// Gamma curve selected 
	lcd_data_write_data(0x01);

	lcd_data_write_command(0xE0);	// Set Gamma 
	lcd_data_write_data(0x0F);
	lcd_data_write_data(0x1D);
	lcd_data_write_data(0x19);
	lcd_data_write_data(0x0E);
	lcd_data_write_data(0x10);
	lcd_data_write_data(0x07);
	lcd_data_write_data(0x4C);
	lcd_data_write_data(0X63);
	lcd_data_write_data(0x3F);
	lcd_data_write_data(0x03);
	lcd_data_write_data(0x0D);
	lcd_data_write_data(0x00);
	lcd_data_write_data(0x26);
	lcd_data_write_data(0x24);
	lcd_data_write_data(0x04);

	lcd_data_write_command(0XE1);	// Set Gamma 
	lcd_data_write_data(0x00);
	lcd_data_write_data(0x1C);
	lcd_data_write_data(0x1F);
	lcd_data_write_data(0x02);
	lcd_data_write_data(0x0F);
	lcd_data_write_data(0x03);
	lcd_data_write_data(0x35);
	lcd_data_write_data(0x25);
	lcd_data_write_data(0x47);
	lcd_data_write_data(0x04);
	lcd_data_write_data(0x0C);
	lcd_data_write_data(0x0B);
	lcd_data_write_data(0x29);
	lcd_data_write_data(0x2F);
	lcd_data_write_data(0x05);

	lcd_data_write_command(0x11);	// Exit Sleep 
	delay_ms(120);
	lcd_data_write_command(0x29);	// Display on

	// Turn on Tearing Effect Line (TE) output signal.
	lcd_data_write_command(0x35);
	lcd_data_write_data(0b00000000);
}

void lcd_init(lcd_t* const lcd) {
	lcd_reset(lcd);
	portapack_init_lcd_kingtech();
	portapack_lcd_backlight(true);
}

void lcd_clear(const lcd_t* const lcd) {
	lcd_clear_rectangle(lcd, 0, 0, lcd->size.w, lcd->size.h);
}

void lcd_set_scroll_area(
	lcd_t* const lcd,
	const uint_fast16_t top_y,
	const uint_fast16_t bottom_y
) {
	lcd->scroll.top_area = top_y;
	lcd->scroll.bottom_area = lcd->size.h - bottom_y;
	lcd->scroll.height = bottom_y - top_y;
	lcd_vertical_scrolling_definition(lcd->scroll.top_area, lcd->scroll.height, lcd->scroll.bottom_area);
}

uint_fast16_t lcd_scroll_area_y(
	lcd_t* const lcd,
	const uint_fast16_t y
) {
	const uint_fast16_t wrapped_y = (lcd->scroll.current_position + y) % lcd->scroll.height;
	return wrapped_y + lcd->scroll.top_area;
}

uint_fast16_t lcd_scroll(
	lcd_t* const lcd,
	const int16_t delta
) {
	lcd->scroll.current_position = (lcd->scroll.current_position + lcd->scroll.height - delta) % lcd->scroll.height;
	lcd_vertical_scrolling_start_address(lcd->scroll.top_area + lcd->scroll.current_position);

	return lcd_scroll_area_y(lcd, 0);
}
