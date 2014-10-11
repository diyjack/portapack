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

#include "lcd_loop.h"

#include <libopencm3/cm3/nvic.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "portapack.h"
#include "portapack_driver.h"

#include "lcd.h"
#include "lcd_color_lut.h"
#include "font_fixed_8x16.h"

#include "console.h"

#include "rtc.h"
#include "ritimer.h"
#include "sdio.h"
#include "rssi.h"
#include <led.h>

#include <stdio.h>

#include "ipc.h"
#include "ipc_m0_client.h"
#include "ipc_m4_client.h"

#include <array>
#include <algorithm>

//#define CPU_METRICS

extern "C" {
#include <libopencm3/lpc43xx/sdio.h>
}

extern "C" {
#include <ff.h>
#include <diskio.h>
}

#if 0
#define DEBUG_SDIO_RESULT(__s, __result) \
	console_write_uint32(&console, __s, __result);
#define DEBUG_FATFS_FRESULT(__s, __fresult) \
	console_write_uint32(&console, __s, __fresult);
#define DEBUG_FATFS_FSIZE(__s, __fsize);
	console_write_uint32(&console, __s, __fsize);
#else
#define DEBUG_SDIO_RESULT(__x, __result)
#define DEBUG_FATFS_FRESULT(__x, __fresult)
#define DEBUG_FATFS_FSIZE(__s, __fsize)
#endif

static FATFS fatfs_sd;
static FIL f_log;

static volatile uint32_t rssi_raw_avg = 0;
static volatile uint32_t rssi_raw_peak = 0;

lcd_t lcd = {
	.size = { .w = 240, .h = 320 },
	.scroll = {
		.top_area = 0,
		.bottom_area = 0,
		.height = 0,
		.current_position = 0,
	},
	.colors = {
		.background = LCD_COLOR(0, 0, 0),
		.foreground = LCD_COLOR(255, 255, 255),
	},
	.font = &font_fixed_8x16,
};

static console_t console;

static void draw_int(int32_t value, const char* const format, uint_fast16_t x, uint_fast16_t y) {
	char temp[80];
	const size_t temp_len = 79;
	const size_t text_len = sprintf(temp, format, value);
	lcd_draw_string(&lcd, x, y, temp, std::min(text_len, temp_len));
}

static void draw_str(const char* const value, const char* const format, uint_fast16_t x, uint_fast16_t y) {
	char temp[80];
	const size_t temp_len = 79;
	const size_t text_len = sprintf(temp, format, value);
	lcd_draw_string(&lcd, x, y, temp, std::min(text_len, temp_len));
}

static void draw_mhz(int64_t value, const char* const format, uint_fast16_t x, uint_fast16_t y) {
	char temp[80];
	const size_t temp_len = 79;
	const int32_t value_mhz = value / 1000000;
	const int32_t value_hz = (value - (value_mhz * 1000000ULL)) / 1000;
	const size_t text_len = sprintf(temp, format, value_mhz, value_hz);
	lcd_draw_string(&lcd, x, y, temp, std::min(text_len, temp_len));
}
#ifdef CPU_METRICS
static void draw_percent(int32_t value_millipercent, const char* const format, uint_fast16_t x, uint_fast16_t y) {
	char temp[80];
	const size_t temp_len = 79;
	const int32_t value_units = value_millipercent / 1000;
	const int32_t value_millis = (value_millipercent - (value_units * 1000)) / 100;
	const size_t text_len = sprintf(temp, format, value_units, value_millis);
	lcd_draw_string(&lcd, x, y, temp, std::min(text_len, temp_len));
}

static void draw_cycles(const uint_fast16_t x, const uint_fast16_t y) {
	lcd_colors_invert(&lcd);
	lcd_draw_string(&lcd, x, y, "Cycle Count ", 12);
	lcd_colors_invert(&lcd);

	const dsp_metrics_t* const metrics = &device_state->dsp_metrics;
	draw_int(metrics->duration_decimate,       "Decim %6d", x, y + 16);
	draw_int(metrics->duration_channel_filter, "Chan  %6d", x, y + 32);
	draw_int(metrics->duration_demodulate,     "Demod %6d", x, y + 48);
	draw_int(metrics->duration_audio,          "Audio %6d", x, y + 64);
	draw_int(metrics->duration_all,            "Total %6d", x, y + 80);
	draw_percent(metrics->duration_all_millipercent, "CPU   %3d.%01d%%", x, y + 96);
}
#endif

typedef struct ui_point_t {
	int16_t x;
	int16_t y;
} ui_point_t;

struct ui_widget_t;

static const ui_widget_t* selected_widget = NULL;

enum ui_event_t {
	UI_EVENT_VALUE_UP,
	UI_EVENT_VALUE_DOWN,
};

typedef void (*ui_widget_value_change_callback_t)();

typedef struct ui_widget_value_change_t {
	ui_widget_value_change_callback_t up;
	ui_widget_value_change_callback_t down;
} ui_widget_value_change_t;

enum ui_widget_flags_t {
	UI_WIDGET_FLAGS_FOCUS = 1,
};

struct ui_widget_t {
	lcd_position_t position;
	lcd_size_t size;
	ui_widget_flags_t flags;
	ui_widget_value_change_t value_change;
	const char* const format;
	const void* (*getter)();
	void (*render_fn)(const ui_widget_t* const);

	void render() const {
		if( selected() ) {
			lcd_colors_invert(&lcd);
		}

		render_fn(this);

		if( selected() ) {
			lcd_colors_invert(&lcd);
		}
	}

	void handle_event(const ui_event_t event) const {
		switch(event) {
		case UI_EVENT_VALUE_UP:
			if( value_change.up ) {
				value_change.up();
			}
			break;

		case UI_EVENT_VALUE_DOWN:
			if( value_change.down ) {
				value_change.down();
			}
			break;
		}
	}

	bool selected() const {
		return this == selected_widget;
	}

	ui_point_t center() const {
		const ui_point_t result = {
			.x = (int16_t)(position.x + (size.w / 2)),
			.y = (int16_t)(position.y + (size.h / 2)),
		};
		return result;
	}
};

static void render_field_mhz(const ui_widget_t* const widget) {
	const int64_t value = *((int64_t*)widget->getter());
	draw_mhz(value, widget->format, widget->position.x, widget->position.y);
}

static void render_field_int(const ui_widget_t* const widget) {
	const int32_t value = *((int32_t*)widget->getter());
	draw_int(value, widget->format, widget->position.x, widget->position.y);
}

static void render_field_str(const ui_widget_t* const widget) {
	const char* const value = (char*)widget->getter();
	draw_str(value, widget->format, widget->position.x, widget->position.y);
}

static void render_field_cpu(const ui_widget_t* const widget) {
	const dsp_metrics_t* const metrics = &device_state->dsp_metrics;
	const int32_t bar_x = std::min(metrics->duration_all_millipercent / 1000, (uint32_t)widget->size.w);
	lcd_fill_rectangle(&lcd,
		widget->position.x, widget->position.y,
		bar_x, widget->size.h
	);
	const lcd_color_t old_foreground = lcd_set_foreground(&lcd, color_black);
	lcd_fill_rectangle(&lcd,
		widget->position.x + bar_x, widget->position.y,
		widget->size.w - bar_x, widget->size.h
	);
	lcd_set_foreground(&lcd, old_foreground);
}

static void render_field_rssi(const ui_widget_t* const widget) {
	const int32_t bar_x = rssi_raw_peak / 16;
	lcd_fill_rectangle(&lcd,
		widget->position.x, widget->position.y,
		bar_x, widget->size.h
	);
	const lcd_color_t old_foreground = lcd_set_foreground(&lcd, color_black);
	lcd_fill_rectangle(&lcd,
		widget->position.x + bar_x, widget->position.y,
		widget->size.w - bar_x, widget->size.h
	);
	lcd_set_foreground(&lcd, old_foreground);
}

static const void* get_tuned_hz() {
	return &device_state->tuned_hz;
}

static const void* get_lna_gain() {
	return &device_state->lna_gain_db;
}

static const void* get_if_gain() {
	return &device_state->if_gain_db;
}

static const void* get_bb_gain() {
	return &device_state->bb_gain_db;
}

static const void* get_audio_out_gain() {
	return &device_state->audio_out_gain_db;
}

static const void* get_receiver_configuration_name() {
	// TODO: Pull this from a shared structure somewhere!
	switch(device_state->receiver_configuration_index) {
	case 0: return "SPEC    ";
	case 1: return "NBAM    ";
	case 2: return "NBFM    ";
	case 3: return "WBFM    ";
	case 4: return "TPMS-ASK";
	case 5: return "TPMS-FSK";
	default: return "????";
	}
}

struct tuning_step_size_t {
	const uint32_t step_hz;
	const char* const name;
};

static const std::array<tuning_step_size_t, 6> tuning_step_sizes { {
	{     1000, "  1kHz" },
	{    10000, " 10kHz" },
	{    25000, " 25kHz" },
	{   100000, "100kHz" },
	{  1000000, "  1MHz" },
	{ 10000000, " 10MHz" },
} };

size_t tuning_step_size_index = 1;

static const tuning_step_size_t* get_tuning_step_size() {
	return &tuning_step_sizes[tuning_step_size_index];
}

static uint32_t get_tuning_step_size_hz() {
	return get_tuning_step_size()->step_hz;
}

static const void* get_tuning_step_size_name() {
	return get_tuning_step_size()->name;
}

static void ui_field_value_up_frequency() {
	ipc_command_set_frequency(&device_state->ipc_m4, device_state->tuned_hz + get_tuning_step_size_hz());
}

static void ui_field_value_down_frequency() {
	ipc_command_set_frequency(&device_state->ipc_m4, device_state->tuned_hz - get_tuning_step_size_hz());
}

static void ui_field_value_up_rf_gain() {
	ipc_command_set_rf_gain(&device_state->ipc_m4, device_state->lna_gain_db + 14);
}

static void ui_field_value_down_rf_gain() {
	ipc_command_set_rf_gain(&device_state->ipc_m4, device_state->lna_gain_db - 14);
}

static void ui_field_value_up_if_gain() {
	ipc_command_set_if_gain(&device_state->ipc_m4, device_state->if_gain_db + 8);
}

static void ui_field_value_down_if_gain() {
	ipc_command_set_if_gain(&device_state->ipc_m4, device_state->if_gain_db - 8);
}

static void ui_field_value_up_bb_gain() {
	ipc_command_set_bb_gain(&device_state->ipc_m4, device_state->bb_gain_db + 2);
}

static void ui_field_value_down_bb_gain() {
	ipc_command_set_bb_gain(&device_state->ipc_m4, device_state->bb_gain_db - 2);
}

static void ui_field_value_up_audio_out_gain() {
	ipc_command_set_audio_out_gain(&device_state->ipc_m4, device_state->audio_out_gain_db + 1);
}

static void ui_field_value_down_audio_out_gain() {
	ipc_command_set_audio_out_gain(&device_state->ipc_m4, device_state->audio_out_gain_db - 1);
}

static void ui_field_value_up_receiver_configuration() {
	ipc_command_set_receiver_configuration(&device_state->ipc_m4, device_state->receiver_configuration_index + 1);
	console_init(&console, &lcd, 16 * 6, lcd.size.h);
}

static void ui_field_value_down_receiver_configuration() {
	ipc_command_set_receiver_configuration(&device_state->ipc_m4, device_state->receiver_configuration_index - 1);
	console_init(&console, &lcd, 16 * 6, lcd.size.h);
}

static void ui_field_value_up_tuning_step_size() {
	tuning_step_size_index = (tuning_step_size_index + 1) % tuning_step_sizes.size();
}

static void ui_field_value_down_tuning_step_size() {
	tuning_step_size_index = (tuning_step_size_index + tuning_step_sizes.size() - 1) % tuning_step_sizes.size();
}

static const ui_widget_t ui_field_frequency {
	{ 0 * 8, 2 * 16 },
	{ 12 * 8, 16 },
	UI_WIDGET_FLAGS_FOCUS,
	{
		ui_field_value_up_frequency,
		ui_field_value_down_frequency,
	},
  	"%4d.%03d MHz",
  	get_tuned_hz,
  	render_field_mhz,
};

static const ui_widget_t ui_field_lna_gain {
	{ 21 * 8, 1 * 16 },
	{ 9 * 8, 16 },
	UI_WIDGET_FLAGS_FOCUS,
	{
		ui_field_value_up_rf_gain,
		ui_field_value_down_rf_gain,
	},
	"LNA %2d dB",
	get_lna_gain,
	render_field_int,
};

static const ui_widget_t ui_field_if_gain {
	{ 21 * 8, 2 * 16 },
	{ 9 * 8, 16 },
	UI_WIDGET_FLAGS_FOCUS,
	{
		ui_field_value_up_if_gain,
		ui_field_value_down_if_gain,
	},
	"IF  %2d dB",
	get_if_gain,
	render_field_int,
};

static const ui_widget_t ui_field_bb_gain {
	{ 21 * 8, 4 * 16 },
	{ 9 * 8, 16 },
	UI_WIDGET_FLAGS_FOCUS,
	{
		ui_field_value_up_bb_gain,
		ui_field_value_down_bb_gain,
	},
	"BB  %2d dB",
	get_bb_gain,
	render_field_int,
};

static const ui_widget_t ui_field_receiver_configuration {
	{ 0 * 8, 1 * 16 },
	{ 13 * 8, 16 },
	UI_WIDGET_FLAGS_FOCUS,
	{
		ui_field_value_up_receiver_configuration,
		ui_field_value_down_receiver_configuration,
	},
	"Mode %8s",
	get_receiver_configuration_name,
	render_field_str,
};

static const ui_widget_t ui_field_tuning_step_size {
	{ 0 * 8, 3 * 16 },
	{ 11 * 8, 16 },
	UI_WIDGET_FLAGS_FOCUS,
	{
		ui_field_value_up_tuning_step_size,
		ui_field_value_down_tuning_step_size,
	},
	"Step %6s",
	get_tuning_step_size_name,
	render_field_str,
};

static const ui_widget_t ui_field_audio_out_gain {
	{ 20 * 8, 5 * 16 },
	{ 10 * 8, 16 },
	UI_WIDGET_FLAGS_FOCUS,
	{
		ui_field_value_up_audio_out_gain,
		ui_field_value_down_audio_out_gain,
	},
	"Vol %3d dB",
	get_audio_out_gain,
	render_field_int,
};

static const ui_widget_t ui_cpu_bar {
	{ 0 * 8, 4 * 16 },
	{ 13 * 8, 16 },
	(ui_widget_flags_t)0,
	{
		nullptr,
		nullptr
	},
	nullptr,
	nullptr,
	render_field_cpu,
};

static const ui_widget_t ui_rssi_bar {
	{ 21 * 8, 3 * 16 },
	{ 9 * 8, 16 },
	(ui_widget_flags_t)0,
	{
		nullptr,
		nullptr,
	},
	nullptr,
	nullptr,
	render_field_rssi,
};

static const std::array<const ui_widget_t*, 9> widgets {
	&ui_rssi_bar,
	&ui_cpu_bar,
	&ui_field_frequency,
	&ui_field_lna_gain,
	&ui_field_if_gain,
	&ui_field_bb_gain,
	&ui_field_receiver_configuration,
	&ui_field_tuning_step_size,
	&ui_field_audio_out_gain,
};

static void ui_widget_update_focus(const ui_widget_t* const focus_widget) {
	if( focus_widget == 0 ) {
		return;
	}
	if( focus_widget == selected_widget ) {
		return;
	}

	const ui_widget_t* const old_widget = selected_widget;
	selected_widget = focus_widget;
	old_widget->render();
	selected_widget->render();
}

typedef enum {
	UI_DIRECTION_DOWN = 0b00,
	UI_DIRECTION_RIGHT = 0b01,
	UI_DIRECTION_LEFT = 0b10,
	UI_DIRECTION_UP = 0b11,
} ui_direction_t;

static int32_t ui_widget_is_above(const ui_point_t start, const ui_point_t end) {
	return (end.y < start.y) ? abs(end.x - start.x) : -1;
}

static int32_t ui_widget_is_below(const ui_point_t start, const ui_point_t end) {
	return (end.y > start.y) ? abs(end.x - start.x) : -1;
}

static int32_t ui_widget_is_left(const ui_point_t start, const ui_point_t end) {
	return (end.x < start.x) ? abs(end.y - start.y) : -1;
}

static int32_t ui_widget_is_right(const ui_point_t start, const ui_point_t end) {
	return (end.x > start.x) ? abs(end.y - start.y) : -1;
}

typedef int32_t (*ui_widget_compare_fn)(const ui_point_t start, const ui_point_t end);

static const ui_widget_compare_fn nearest_widget_fn[] = {
	[UI_DIRECTION_DOWN] = &ui_widget_is_below,
	[UI_DIRECTION_RIGHT] = &ui_widget_is_right,
	[UI_DIRECTION_LEFT] = &ui_widget_is_left,
	[UI_DIRECTION_UP] = &ui_widget_is_above,
};

static const ui_widget_t* ui_widget_find_nearest(const ui_widget_t* const widget, const ui_direction_t desired_direction) {
	const ui_point_t source_point = widget->center();
	int32_t nearest_distance = 0;
	const ui_widget_t* nearest_widget = 0;
	for(const auto other_widget : widgets) {
		if( other_widget == widget ) {
			continue;
		}
		if( (other_widget->flags & UI_WIDGET_FLAGS_FOCUS) == 0 ) {
			continue;
		}

		const ui_point_t target_point = other_widget->center();
		const int32_t distance = nearest_widget_fn[desired_direction](source_point, target_point);
		if( distance > -1 ) {
			if( (nearest_widget == 0) || (distance < nearest_distance) ) {
				nearest_distance = distance;
				nearest_widget = other_widget;
			}
		}
	}

	return nearest_widget;
}

static void ui_widget_navigate_up() {
	ui_widget_update_focus(ui_widget_find_nearest(selected_widget, UI_DIRECTION_UP));
}

static void ui_widget_navigate_down() {
	ui_widget_update_focus(ui_widget_find_nearest(selected_widget, UI_DIRECTION_DOWN));
}

static void ui_widget_navigate_left() {
	ui_widget_update_focus(ui_widget_find_nearest(selected_widget, UI_DIRECTION_LEFT));
}

static void ui_widget_navigate_right() {
	ui_widget_update_focus(ui_widget_find_nearest(selected_widget, UI_DIRECTION_RIGHT));
}

static void ui_widget_value_up() {
	selected_widget->handle_event(UI_EVENT_VALUE_UP);
}

static void ui_widget_value_down() {
	selected_widget->handle_event(UI_EVENT_VALUE_DOWN);
}

static bool ui_widget_hit(const ui_widget_t* const widget, const uint_fast16_t x, const uint_fast16_t y) {
	if( (x >= widget->position.x) && (x < (widget->position.x + widget->size.w)) &&
		(y >= widget->position.y) && (y < (widget->position.y + widget->size.h)) ) {
		return true;
	} else {
		return false;
	}
}

static const ui_widget_t* ui_widgets_hit(const uint_fast16_t x, const uint_fast16_t y) {
	for(const auto widget : widgets) {
		if( ui_widget_hit(widget, x, y) ) {
			return widget;
		}
	}
	return NULL;
}

static void ui_render_widgets() {
	for(const auto widget : widgets) {
		widget->render();
	}
}

typedef struct ui_switch_t {
	uint32_t mask;
	void (*action)();
} ui_switch_t;

void switch_select() {
}

static const std::array<ui_switch_t, 5> switches { {
	{ SWITCH_UP,     ui_widget_navigate_up },
	{ SWITCH_DOWN,   ui_widget_navigate_down },
	{ SWITCH_LEFT,   ui_widget_navigate_left },
	{ SWITCH_RIGHT,  ui_widget_navigate_right },
	{ SWITCH_SELECT, switch_select },
} };

static bool handle_joysticks() {
	static uint32_t switches_history[3] = { 0, 0, 0 };
	static uint32_t switches_last = 0;

	static int32_t last_encoder_position = 0;
	const int32_t current_encoder_position = device_state->encoder_position;
	const int32_t encoder_inc = current_encoder_position - last_encoder_position;
	last_encoder_position = current_encoder_position;

	bool event_occurred = false;
	if( encoder_inc > 0 ) {
		ui_widget_value_up();
		event_occurred = true;
	}
	if( encoder_inc < 0 ) {
		ui_widget_value_down();
		event_occurred = true;
	}
	
	const uint32_t switches_raw = portapack_read_switches();
	const uint32_t switches_now = switches_raw & switches_history[0] & switches_history[1] & switches_history[2];
	switches_history[0] = switches_history[1];
	switches_history[1] = switches_history[2];
	switches_history[2] = switches_raw;

	const uint32_t switches_event = switches_now ^ switches_last;
	const uint32_t switches_event_on = switches_event & switches_now;
	switches_last = switches_now;

	for(const auto& sw : switches) {
		if( switches_event_on & sw.mask ) {
			sw.action();
			event_occurred = true;
		}
	}

	return event_occurred;
}

#include "lcd_touch.h"

static void draw_rtc(const uint_fast16_t x, const uint_fast16_t y) {
	draw_int(rtc_year(), "%04d/", x + 0*8, y);
	draw_int(rtc_month(), "%02d/", x + 5*8, y);
	draw_int(rtc_day_of_month(), "%02d ", x + 8*8, y);
	draw_int(rtc_hour(), "%02d:", x + 11*8, y);
	draw_int(rtc_minute(), "%02d:", x + 14*8, y);
	draw_int(rtc_second(), "%02d", x + 17*8, y);
}

static void sdio_draw_error(const sdio_error_t error) {
	switch(error) {
	case SDIO_OK:
	 	console_writeln(&console, "OK");
	 	break;

	case SDIO_ERROR_HARDWARE_IS_LOCKED:
		console_writeln(&console, "HLE");
		break;

	case SDIO_ERROR_RESPONSE_TIMED_OUT:
		console_writeln(&console, "TO");
		break;

	case SDIO_ERROR_RESPONSE_CRC_ERROR:
		console_writeln(&console, "CE");
		break;

	case SDIO_ERROR_RESPONSE_ERROR:
		console_writeln(&console, "RE");
		break;

	case SDIO_ERROR_RESPONSE_ON_INITIALIZATION:
		console_writeln(&console, "OI");
		break;

	case SDIO_ERROR_RESPONSE_CHECK_PATTERN_INCORRECT:
		console_writeln(&console, "CPI");
		break;

	case SDIO_ERROR_RESPONSE_VOLTAGE_NOT_ACCEPTED:
		console_writeln(&console, "VNA");
		break;

	default:
		console_writeln(&console, "??");
		break;
	}
}

static DSTATUS sdio_status = STA_NODISK | STA_NOINIT;

static sdio_error_t sdio_try_sd_version_2() {
	sdio_error_t result_acmd41;
	for(size_t i=0; i<10; i++) {
		const uint32_t hcs = 1;
		result_acmd41 = sdio_acmd41(hcs);
		console_write_uint32(&console, "ACMD41(hcs=%d)", hcs);
		console_write_uint32(&console, " %08x ", SDIO_RESP0);
		sdio_draw_error(result_acmd41);
		
		if( result_acmd41 == SDIO_OK ) {
			const bool ccs = (SDIO_RESP0 >> 30) & 1;
			if( ccs ) {
				// High- or Extended-capacity
				console_writeln(&console, "Density: HC/XC");
			} else {
				// Standard capacity
				console_writeln(&console, "Density: SD");
			}
			// Ignore S18R/S18A, we can't switch voltage.

			sdio_cclk_set_20mhz();

			sdio_error_t result_cmd2 = sdio_cmd2();
			console_write(&console, "CMD2 ");
			sdio_draw_error(result_cmd2);
			if( result_cmd2 == SDIO_OK ) {
				console_write_uint32(&console, "%08x", SDIO_RESP3);
				console_write_uint32(&console, " %08x", SDIO_RESP2);
				console_writeln(&console, "");
				console_write_uint32(&console, "%08x", SDIO_RESP1);
				console_write_uint32(&console, " %08x", SDIO_RESP0);
				console_writeln(&console, "");

				sdio_error_t result_cmd3 = sdio_cmd3();
				console_write(&console, "CMD3 ");
				sdio_draw_error(result_cmd3);
				if( result_cmd3 == SDIO_OK ) {
					const uint32_t rca = SDIO_RESP0 >> 16;
					const uint32_t status = SDIO_RESP0 & 0xffff;
					console_write_uint32(&console, "RCA: %04x ", rca);
					console_writeln(&console, "");
					console_write_uint32(&console, "Status: %04x ", status);
					console_writeln(&console, "");

					sdio_error_t result_cmd7 = sdio_cmd7(rca);
					console_write(&console, "CMD7 ");
					sdio_draw_error(result_cmd7);
					if( result_cmd7 == SDIO_OK ) {
						sdio_status &= ~STA_NOINIT;
					}
					return result_cmd7;
				}
				return result_cmd3;
			} else {
				return result_cmd2;
			}
		}
		delay(10000000);
	}
	return result_acmd41;
}

static sdio_error_t sdio_try_sd_version_1() {
	/* TODO: Unimplemented */
	return SDIO_ERROR_RESPONSE_ERROR;
}

sdio_error_t sdio_enumerate_card_stack() {
	sdio_status |= STA_NOINIT;

	sdio_cclk_set_400khz();
	sdio_set_width_1bit();

	sdio_error_t result_cmd0 = sdio_cmd0(1);
	console_write(&console, "CMD0(1)");
	console_write_uint32(&console, " %08x ", SDIO_RESP0);
	sdio_draw_error(result_cmd0);

	// Interface condition
	sdio_error_t result_cmd8 = sdio_cmd8();
	console_write(&console, "CMD8");
	console_write_uint32(&console, " %08x ", SDIO_RESP0);
	sdio_draw_error(result_cmd8);

	switch(result_cmd8) {
	case SDIO_OK:
		return sdio_try_sd_version_2();

	case SDIO_ERROR_RESPONSE_CHECK_PATTERN_INCORRECT:
		console_writeln(&console, "Unknown card");
		return result_cmd8;

	default:
		return sdio_try_sd_version_1();
	}
}

DSTATUS disk_initialize(BYTE pdrv) {	
	(void)pdrv;

	sdio_enumerate_card_stack();
	DEBUG_SDIO_RESULT("[i%x]", sdio_status);
	return sdio_status;
}

DSTATUS disk_status(BYTE pdrv) {
	(void)pdrv;

	return sdio_status;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
	(void)pdrv;

	if( count > 1 ) {
		return RES_ERROR;
	}

	sdio_error_t result = sdio_read(sector, (uint32_t*)buff, count);
	DEBUG_SDIO_RESULT("[r%d]", result);

	return (result == SDIO_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
	(void)pdrv;

	if( count > 1 ) {
		return RES_ERROR;
	}

	sdio_error_t result = sdio_write(sector, (uint32_t*)buff, count);
	DEBUG_SDIO_RESULT("[w%d]", result);

	return (result == SDIO_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
	(void)pdrv;
	(void)buff;

	switch(cmd) {
	case CTRL_SYNC:
		/* At present, no cached data */
		return RES_OK;

	default:
		console_write_uint32(&console, "[c%d]", cmd);
		return RES_ERROR;
	}
}

DWORD get_fattime(void) {
	uint32_t fattime = 0;
	fattime |= std::max(0L, (int32_t)rtc_year() - 1980);
	fattime <<= 4;
	fattime |= rtc_month();
	fattime <<= 5;
	fattime |= rtc_day_of_month();
	fattime <<= 5;
	fattime |= rtc_hour();
	fattime <<= 6;
	fattime |= rtc_minute();
	fattime <<= 5;
	fattime |= rtc_second() >> 1;
	return fattime;
}

static void touch_started(const touch_state_t* const state) {
	const ui_widget_t* const hit_widget = ui_widgets_hit(state->x, state->y);
	ui_widget_update_focus(hit_widget);
}

static void touch_ended(const touch_state_t* const state) {
	(void)state;
}

static void handle_touch_events(const touch_state_t* const state) {
	static bool touching = false;
	if( state->pressure ) {
		if( touching == false ) {
			touching = true;
			touch_started(state);
		}

		lcd_set_pixel(&lcd, state->x, state->y);
	} else {
		if( touching == true ) {
			touching = false;
			touch_ended(state);
		}
	}
}

typedef enum {
	UI_BUTTON_STATE_NORMAL,
	UI_BUTTON_STATE_TOUCHED,
} ui_button_state_t;

typedef struct ui_button_t {
	lcd_position_t position;
	lcd_size_t size;
	const char* const label;
	ui_button_state_t state;
} ui_button_t;

static std::array<ui_button_t, 10> numeric_entry { {
	{ .position = { .x =   0, .y =  16 }, .size = { .w = 80, .h = 48 }, .label = "1", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x =  80, .y =  16 }, .size = { .w = 80, .h = 48 }, .label = "2", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x = 160, .y =  16 }, .size = { .w = 80, .h = 48 }, .label = "3", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x =   0, .y =  64 }, .size = { .w = 80, .h = 48 }, .label = "4", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x =  80, .y =  64 }, .size = { .w = 80, .h = 48 }, .label = "5", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x = 160, .y =  64 }, .size = { .w = 80, .h = 48 }, .label = "6", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x =   0, .y = 112 }, .size = { .w = 80, .h = 48 }, .label = "7", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x =  80, .y = 112 }, .size = { .w = 80, .h = 48 }, .label = "8", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x = 160, .y = 112 }, .size = { .w = 80, .h = 48 }, .label = "9", .state = UI_BUTTON_STATE_NORMAL },
	{ .position = { .x =  80, .y = 160 }, .size = { .w = 80, .h = 48 }, .label = "0", .state = UI_BUTTON_STATE_NORMAL },
} };

static void ui_button_render(const ui_button_t* const button) {
	if( button->state == UI_BUTTON_STATE_TOUCHED ) {
		lcd_colors_invert(&lcd);
	}

	lcd_draw_filled_rectangle(&lcd, button->position.x, button->position.y, button->size.w, button->size.h);
	const size_t label_length = strlen(button->label);
	lcd_draw_string(&lcd, button->position.x + (button->size.w - label_length * 8) / 2, button->position.y + (button->size.h - 16) / 2, button->label, label_length);

	if( button->state == UI_BUTTON_STATE_TOUCHED ) {
		lcd_colors_invert(&lcd);
	}	
}

void ui_render_numeric_entry() {
	for(const auto& button : numeric_entry) {
		ui_button_render(&button);
	}
}

bool ui_button_hit(const ui_button_t* const button, const uint_fast16_t x, const uint_fast16_t y) {
	if( (x >= button->position.x) && (x < (button->position.x + button->size.w)) &&
		(y >= button->position.y) && (y < (button->position.y + button->size.h)) ) {
		return true;
	} else {
		return false;
	}
}

ui_button_t* ui_numeric_entry_touched(const touch_state_t* const touch) {
	if( touch->pressure ) {
		for(auto& button : numeric_entry) {
			if( ui_button_hit(&button, touch->x, touch->y) ) {
				return &button;
			}
		}
	}
	return NULL;
}

#include "ipc_m0.h"
#include "ipc_m0_server.h"

#include "manchester.h"

void handle_command_packet_data_received_ask(const void* const arg) {
	const ipc_command_packet_data_received_t* const command = (ipc_command_packet_data_received_t*)arg;

	uint8_t value[5];
	uint8_t errors[5];
	manchester_decode(command->payload, value, errors, 37);

	const uint_fast8_t flag_group_1[] = {
		(uint_fast8_t)(value[0] >> 7) & 1,
		(uint_fast8_t)(value[0] >> 6) & 1,
		(uint_fast8_t)(value[0] >> 5) & 1,
	};
	const uint32_t id = ((((((value[0] & 0x1f) << 8) | value[1]) << 8) | value[2]) << 3) | (value[3] >> 5);
	const uint32_t pressure = ((value[3] & 0x1f) << 3) | (value[4] >> 5);
	const uint_fast8_t flag_group_2[] = {
		(uint_fast8_t)(value[4] >> 4) & 1,
		(uint_fast8_t)(value[4] >> 3) & 1,
	};
	console_write_uint32(&console, "%1u", flag_group_1[0]);
	console_write_uint32(&console, "%1u", flag_group_1[1]);
	console_write_uint32(&console, "%1u", flag_group_1[2]);
	console_write_uint32(&console, " %8u", id);
	console_write_uint32(&console, " %3u", pressure);
	console_write_uint32(&console, " %1u", flag_group_2[0]);
	console_write_uint32(&console, "%1u/", flag_group_2[1]);
	for(size_t i=0; i<5; i++) {
		console_write_uint32(&console, "%02x", errors[i]);
	}
	console_writeln(&console, "");
}

static void set_console_error_color(const uint32_t error) {
	if( error ) {
		console_set_background(&console, color_red);
	} else {
		console_set_background(&console, color_black);
	}
}

static void log_buffer(const char* const message, const size_t length) {
	UINT bytes_written = 0;
	f_write(&f_log, message, length, &bytes_written);
}

static void log_string(const char* const message) {
	log_buffer(message, strlen(message));
}

static void log_bytes_hex(const uint8_t* const buffer, const size_t count) {
	/* TODO: Handle count > buffer size */
	char tmp[80];
	for(size_t i=0; i<count; i++) {
		sprintf(&tmp[i*2], "%02x", buffer[i]);
	}
	log_buffer(tmp, count * 2);
}

static void log_timestamp() {
	char tmp[80];
	sprintf(tmp, "%04d/%02d/%02d %02d:%02d:%02d",
		rtc_year(),
		rtc_month(),
		rtc_day_of_month(),
		rtc_hour(),
		rtc_minute(),
		rtc_second()
	);
	log_string(tmp);
}

void handle_command_packet_data_received_fsk(const void* const arg) {
	const ipc_command_packet_data_received_t* const command = (ipc_command_packet_data_received_t*)arg;

	log_timestamp();
	log_string(" ");

	uint8_t value[10];
	uint8_t errors[10];
	manchester_decode(command->payload, value, errors, 80);

	for(size_t i=0; i<10; i++) {
		set_console_error_color(errors[i] >> 4);
		console_write_uint32(&console, "%01x", value[i] >> 4);
		set_console_error_color(errors[i] & 0xf);
		console_write_uint32(&console, "%01x", value[i] & 0xf);
	}
	console_set_background(&console, color_black);

	log_bytes_hex(value, 10);
	log_string("/");
	log_bytes_hex(errors, 10);
	log_string("\n");

	if( ((value[0] >> 5) == 0b001) ) {
		const uint_fast8_t value_1 = value[6];
		console_write_uint32(&console, " %03d", value_1);
		const uint_fast8_t value_2 = value[7];
		console_write_uint32(&console, " %03d", value_2);
	}

	if( ((value[0] >> 6) == 0b01) ) {
		const uint_fast8_t value_1 = value[5];
		console_write_uint32(&console, " %03d", value_1);
		const uint_fast8_t value_2 = value[6];
		console_write_uint32(&console, " %03d", value_2);
	}

	if( ((value[0] >> 3) == 0b11001) ) {
		const uint_fast8_t value_1 = value[4];
		console_write_uint32(&console, " %03d", value_1);
		const uint_fast8_t value_2 = value[5];
		console_write_uint32(&console, " %03d", value_2);
	}

	if( ((value[0] >> 3) == 0b11110) ) {
		const uint_fast8_t value_1 = value[5];
		console_write_uint32(&console, " %03d", value_1);
		const uint_fast8_t value_2 = value[6];
		console_write_uint32(&console, " %03d", value_2);
	}

	console_writeln(&console, "");
}

void handle_command_packet_data_received(const void* const arg) {
	// TODO: Naughty, hard-coded!
	if( device_state->receiver_configuration_index == 5 ) {
		handle_command_packet_data_received_fsk(arg);
	} else {
		handle_command_packet_data_received_ask(arg);
	}
}

void handle_command_spectrum_data(const void* const arg) {

	const ipc_command_spectrum_data_t* const command = (ipc_command_spectrum_data_t*)arg;
	const uint_fast16_t draw_y = lcd_scroll(&lcd, 1);
	const uint_fast16_t x = 0;
	const uint_fast16_t y = draw_y;

	lcd_start_drawing(x, y, lcd.size.w, 1);
	for(size_t i=0; i<lcd.size.w; i++) {
		const uint_fast16_t bin = (i + (128 + (256 - 240) / 2)) & 0xff;
		const lcd_color_t color = spectrum_rgb3_lut[command->avg[bin]];
		lcd_data_write_rgb(color);
	}

	ipc_command_spectrum_data_done(&device_state->ipc_m4);
}

void handle_command_rtc_second(const void* const arg) {
	(void)arg;
	lcd_colors_invert(&lcd);
	draw_rtc(11 * 8, 0);
	lcd_colors_invert(&lcd);

	f_sync(&f_log);
}

static void ritimer_init_1khz_isr() {
	ritimer_init();
	ritimer_compare_set(200000); /* TODO: Blindly assuming 200MHz -> 1kHz */
	ritimer_match_clear_enable();
	ritimer_enable();

	nvic_set_priority(NVIC_RITIMER_OR_WWDT_IRQ, 255);
	nvic_enable_irq(NVIC_RITIMER_OR_WWDT_IRQ);
}

extern "C" void ritimer_or_wwdt_isr() {
	ritimer_interrupt_clear();
	const uint32_t rssi_raw = rssi_read();
	rssi_convert_start();
	rssi_raw_avg = (rssi_raw_avg * 15 + rssi_raw) / 16;
	if( rssi_raw > rssi_raw_peak ) {
		rssi_raw_peak = rssi_raw;
	} else {
		rssi_raw_peak = (rssi_raw_peak * 15) / 16;
	}
	encoder_update();
}

int main() {
	sdio_init();
	rssi_init();
	lcd_init(&lcd);
	lcd_touch_init();
	portapack_encoder_init();
	ritimer_init_1khz_isr();

	lcd_set_background(&lcd, color_blue);
	lcd_set_foreground(&lcd, color_white);
	lcd_clear(&lcd);

	lcd_colors_invert(&lcd);
	lcd_draw_string(&lcd, 0, 0, "PortaPack                     ", 30);
	lcd_colors_invert(&lcd);

	console_init(&console, &lcd, 16 * 6, lcd.size.h);

	if( sdio_card_is_present() ) {
		for(size_t n=0; n<1000; n++) {
			delay(51000);
		}

		sdio_status &= ~STA_NODISK;

		FRESULT fresult;
		fresult = f_mount(&fatfs_sd, "", 0);
		DEBUG_FATFS_FRESULT("f_mount: %d", fresult);
		fresult = f_open(&f_log, "log.txt", FA_OPEN_ALWAYS | FA_WRITE);
		DEBUG_FATFS_FRESULT("f_open: %d", fresult);
		const size_t f_log_size = f_size(&f_log);
		DEBUG_FATFS_FSIZE("f_size: %u", f_log_size);
		fresult = f_lseek(&f_log, f_log_size);
		DEBUG_FATFS_FRESULT("f_lseek: %d", fresult);
	}

	ipc_command_set_audio_out_gain(&device_state->ipc_m4, 0);

	selected_widget = &ui_field_frequency;

bool numeric_entry = false;

	while(1) {
		const bool sd_card_present = sdio_card_is_present();
		if( sd_card_present ) {
			sdio_status &= ~STA_NODISK;
		} else {
			sdio_status |= STA_NODISK;
		}
#ifdef CPU_METRICS
		draw_cycles(240 - (12 * 8), 96);
#endif

		touch_state_t touch_state;
		lcd_touch_convert(&touch_state);
		handle_touch_events(&touch_state);

		if( numeric_entry ) {
			ui_render_numeric_entry();
			ui_button_t* const button_touched = ui_numeric_entry_touched(&touch_state);
			if( button_touched ) {
				button_touched->state = UI_BUTTON_STATE_TOUCHED;
				ui_button_render(button_touched);
			}
		} else {
			ui_render_widgets();
		}

		while( !ipc_channel_is_empty(&device_state->ipc_m4) );

		handle_joysticks();
		ipc_m0_handle();

		lcd_frame_sync();
	}

	return 0;
}
