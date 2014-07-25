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

#include <stdint.h>
#include <string.h>

#include "portapack.h"
#include "portapack_driver.h"

#include "lcd.h"
#include "lcd_color_lut.h"
#include "font_fixed_8x16.h"

#include "console.h"

#include "rtc.h"
#include "sdio.h"
#include <led.h>

#include <stdio.h>

#include "ipc.h"
#include "ipc_m4_client.h"

#include "linux_stuff.h"

//#define CPU_METRICS

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

void blink() {
	while(1) {
		led_toggle(LED3);
		delay(2000000);
	}
}

static void draw_int(int32_t value, const char* const format, uint_fast16_t x, uint_fast16_t y) {
	char temp[80];
	const size_t temp_len = 79;
	const size_t text_len = snprintf(temp, temp_len, format, value);
	lcd_draw_string(&lcd, x, y, temp, min(text_len, temp_len));
}

static void draw_str(const char* const value, const char* const format, uint_fast16_t x, uint_fast16_t y) {
	char temp[80];
	const size_t temp_len = 79;
	const size_t text_len = snprintf(temp, temp_len, format, value);
	lcd_draw_string(&lcd, x, y, temp, min(text_len, temp_len));
}

static void draw_mhz(int64_t value, const char* const format, uint_fast16_t x, uint_fast16_t y) {
	char temp[80];
	const size_t temp_len = 79;
	const int32_t value_mhz = value / 1000000;
	const int32_t value_hz = (value - (value_mhz * 1000000)) / 1000;
	const size_t text_len = snprintf(temp, temp_len, format, value_mhz, value_hz);
	lcd_draw_string(&lcd, x, y, temp, min(text_len, temp_len));
}
#ifdef CPU_METRICS
static void draw_percent(int32_t value_millipercent, const char* const format, uint_fast16_t x, uint_fast16_t y) {
	char temp[80];
	const size_t temp_len = 79;
	const int32_t value_units = value_millipercent / 1000;
	const int32_t value_millis = (value_millipercent - (value_units * 1000)) / 100;
	const size_t text_len = snprintf(temp, temp_len, format, value_units, value_millis);
	lcd_draw_string(&lcd, x, y, temp, min(text_len, temp_len));
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
struct ui_field_text_t;
typedef struct ui_field_text_t ui_field_text_t;

typedef enum {
	UI_FIELD_FREQUENCY,
	UI_FIELD_LNA_GAIN,
	UI_FIELD_IF_GAIN,
	UI_FIELD_BB_GAIN,
	UI_FIELD_RECEIVER_CONFIGURATION,
	UI_FIELD_TUNING_STEP_SIZE,
	UI_FIELD_AUDIO_OUT_GAIN,
	UI_FIELD_LAST,
} ui_field_index_t;

typedef struct ui_field_navigation_t {
	ui_field_index_t up;
	ui_field_index_t down;
	ui_field_index_t left;
	ui_field_index_t right;
} ui_field_navigation_t;

typedef void (*ui_field_value_change_callback_t)(const uint32_t repeat_count);

typedef struct ui_field_value_change_t {
	ui_field_value_change_callback_t up;
	ui_field_value_change_callback_t down;
} ui_field_value_change_t;

struct ui_field_text_t {
	lcd_position_t position;
	lcd_size_t size;
	ui_field_navigation_t navigation;
	ui_field_value_change_t value_change;
	const char* const format;
	const void* (*getter)();
	void (*render)(const ui_field_text_t* const);
};

static void render_field_mhz(const ui_field_text_t* const field) {
	const int64_t value = *((int64_t*)field->getter());
	draw_mhz(value, field->format, field->position.x, field->position.y);
}

static void render_field_int(const ui_field_text_t* const field) {
	const int32_t value = *((int32_t*)field->getter());
	draw_int(value, field->format, field->position.x, field->position.y);
}

static void render_field_str(const ui_field_text_t* const field) {
	const char* const value = (char*)field->getter();
	draw_str(value, field->format, field->position.x, field->position.y);
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
	case 0: return "SPEC";
	case 1: return "NBAM";
	case 2: return "NBFM";
	case 3: return "WBFM";
	case 4: return "TPMS";
	default: return "????";
	}
}

typedef struct tuning_step_size_t {
	uint32_t step_hz;
	const char* name;
} tuning_step_size_t;

tuning_step_size_t tuning_step_sizes[] = {
	{ .step_hz =    10000, .name = " 10kHz" },
	{ .step_hz =    25000, .name = " 25kHz" },
	{ .step_hz =   100000, .name = "100kHz" },
	{ .step_hz =  1000000, .name = "  1MHz" },
	{ .step_hz = 10000000, .name = " 10MHz" },
};

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
#if 0
static int32_t frequency_repeat_acceleration(const uint32_t repeat_count) {
	int32_t amount = 25000;
	if( repeat_count >= 160 ) {
		amount = 100000000;
	} else if( repeat_count >= 80 ) {
		amount = 10000000;
	} else if( repeat_count >= 40 ) {
		amount = 1000000;
	} else if( repeat_count >= 20 ) {
		amount = 100000;
	}
	return amount;
}
#endif
static void ui_field_value_up_frequency(const uint32_t amount) {
	ipc_command_set_frequency(&device_state->ipc_m4, device_state->tuned_hz + (amount * get_tuning_step_size_hz())/*frequency_repeat_acceleration(repeat_count)*/);
}

static void ui_field_value_down_frequency(const uint32_t amount) {
	ipc_command_set_frequency(&device_state->ipc_m4, device_state->tuned_hz - (amount * get_tuning_step_size_hz())/*frequency_repeat_acceleration(repeat_count)*/);
}

static void ui_field_value_up_rf_gain(const uint32_t amount) {
	(void)amount;
	ipc_command_set_rf_gain(&device_state->ipc_m4, device_state->lna_gain_db + 14);
}

static void ui_field_value_down_rf_gain(const uint32_t amount) {
	(void)amount;
	ipc_command_set_rf_gain(&device_state->ipc_m4, device_state->lna_gain_db - 14);
}

static void ui_field_value_up_if_gain(const uint32_t amount) {
	(void)amount;
	ipc_command_set_if_gain(&device_state->ipc_m4, device_state->if_gain_db + 8);
}

static void ui_field_value_down_if_gain(const uint32_t amount) {
	(void)amount;
	ipc_command_set_if_gain(&device_state->ipc_m4, device_state->if_gain_db - 8);
}

static void ui_field_value_up_bb_gain(const uint32_t amount) {
	(void)amount;
	ipc_command_set_bb_gain(&device_state->ipc_m4, device_state->bb_gain_db + 2);
}

static void ui_field_value_down_bb_gain(const uint32_t amount) {
	(void)amount;
	ipc_command_set_bb_gain(&device_state->ipc_m4, device_state->bb_gain_db - 2);
}

static void ui_field_value_up_audio_out_gain(const uint32_t amount) {
	(void)amount;
	ipc_command_set_audio_out_gain(&device_state->ipc_m4, device_state->audio_out_gain_db + 1);
}

static void ui_field_value_down_audio_out_gain(const uint32_t amount) {
	(void)amount;
	ipc_command_set_audio_out_gain(&device_state->ipc_m4, device_state->audio_out_gain_db - 1);
}

static void ui_field_value_up_receiver_configuration(const uint32_t amount) {
	(void)amount;
	ipc_command_set_receiver_configuration(&device_state->ipc_m4, device_state->receiver_configuration_index + 1);
}

static void ui_field_value_down_receiver_configuration(const uint32_t amount) {
	(void)amount;
	ipc_command_set_receiver_configuration(&device_state->ipc_m4, device_state->receiver_configuration_index - 1);
}

static void ui_field_value_up_tuning_step_size(const uint32_t amount) {
	(void)amount;
	tuning_step_size_index = (tuning_step_size_index + 1) % ARRAY_SIZE(tuning_step_sizes);
}

static void ui_field_value_down_tuning_step_size(const uint32_t amount) {
	(void)amount;
	tuning_step_size_index = (tuning_step_size_index + ARRAY_SIZE(tuning_step_sizes) - 1) % ARRAY_SIZE(tuning_step_sizes);
}

static ui_field_text_t fields[] = {
	[UI_FIELD_FREQUENCY] = {
		.position = { .x = 0, .y = 32 },
		.size = { .w = 12 * 8, .h = 16 },
		.navigation = {
			.up = UI_FIELD_BB_GAIN,
			.down = UI_FIELD_LNA_GAIN,
			.left = UI_FIELD_FREQUENCY,
			.right = UI_FIELD_RECEIVER_CONFIGURATION,
		},
		.value_change = {
			.up = ui_field_value_up_frequency,
			.down = ui_field_value_down_frequency,
		},
	  	.format = "%4d.%03d MHz",
	  	.getter = get_tuned_hz,
	  	.render = render_field_mhz
	},
	[UI_FIELD_LNA_GAIN] = {
		.position = { .x = 0, .y = 48 },
		.size = { .w = 9 * 8, .h = 16 },
		.navigation = {
			.up = UI_FIELD_FREQUENCY,
			.down = UI_FIELD_IF_GAIN,
			.left = UI_FIELD_LNA_GAIN,
			.right = UI_FIELD_TUNING_STEP_SIZE,
		},
		.value_change = {
			.up = ui_field_value_up_rf_gain,
			.down = ui_field_value_down_rf_gain,
		},
		.format = "LNA %2d dB",
		.getter = get_lna_gain,
		.render = render_field_int
	},
	[UI_FIELD_IF_GAIN] = {
		.position = { .x = 0, .y = 64 },
		.size = { .w = 9 * 8, .h = 16 },
		.navigation = {
			.up = UI_FIELD_LNA_GAIN,
			.down = UI_FIELD_BB_GAIN,
			.left = UI_FIELD_IF_GAIN,
			.right = UI_FIELD_AUDIO_OUT_GAIN,
		},
		.value_change = {
			.up = ui_field_value_up_if_gain,
			.down = ui_field_value_down_if_gain,
		},
		.format = "IF  %2d dB",
		.getter = get_if_gain,
		.render = render_field_int
	},
	[UI_FIELD_BB_GAIN] = {
		.position = { .x = 0, .y = 80 },
		.size = { .w = 9 * 8, .h = 16 },
		.navigation = {
			.up = UI_FIELD_IF_GAIN,
			.down = UI_FIELD_FREQUENCY,
			.left = UI_FIELD_BB_GAIN,
			.right = UI_FIELD_AUDIO_OUT_GAIN,
		},
		.value_change = {
			.up = ui_field_value_up_bb_gain,
			.down = ui_field_value_down_bb_gain,
		},
		.format = "BB  %2d dB",
		.getter = get_bb_gain,
		.render = render_field_int
	},
	[UI_FIELD_RECEIVER_CONFIGURATION] = {
		.position = { .x = 128, .y = 32 },
		.size = { .w = 9 * 8, .h = 16 },
		.navigation = {
			.up = UI_FIELD_AUDIO_OUT_GAIN,
			.down = UI_FIELD_TUNING_STEP_SIZE,
			.left = UI_FIELD_FREQUENCY,
			.right = UI_FIELD_RECEIVER_CONFIGURATION,
		},
		.value_change = {
			.up = ui_field_value_up_receiver_configuration,
			.down = ui_field_value_down_receiver_configuration,
		},
		.format = "Mode %4s",
		.getter = get_receiver_configuration_name,
		.render = render_field_str,
	},
	[UI_FIELD_TUNING_STEP_SIZE] = {
		.position = { .x = 128, .y = 48 },
		.size = { .w = 11 * 8, .h = 16 },
		.navigation = {
			.up = UI_FIELD_RECEIVER_CONFIGURATION,
			.down = UI_FIELD_AUDIO_OUT_GAIN,
			.left = UI_FIELD_LNA_GAIN,
			.right = UI_FIELD_TUNING_STEP_SIZE,
		},
		.value_change = {
			.up = ui_field_value_up_tuning_step_size,
			.down = ui_field_value_down_tuning_step_size,
		},
		.format = "Step %6s",
		.getter = get_tuning_step_size_name,
		.render = render_field_str,
	},
	[UI_FIELD_AUDIO_OUT_GAIN] = {
		.position = { .x = 128, .y = 64 },
		.size = { .w = 10 * 8, .h = 16 },
		.navigation = {
			.up = UI_FIELD_TUNING_STEP_SIZE,
			.down = UI_FIELD_RECEIVER_CONFIGURATION,
			.left = UI_FIELD_IF_GAIN,
			.right = UI_FIELD_AUDIO_OUT_GAIN,
		},
		.value_change = {
			.up = ui_field_value_up_audio_out_gain,
			.down = ui_field_value_down_audio_out_gain,
		},
		.format = "Vol %3d dB",
		.getter = get_audio_out_gain,
		.render = render_field_int
	},
};

static ui_field_index_t selected_field = UI_FIELD_FREQUENCY;

static void ui_field_render(const ui_field_index_t field) {
	if( field == selected_field ) {
		lcd_colors_invert(&lcd);
	}

	fields[field].render(&fields[field]);

	if( field == selected_field ) {
		lcd_colors_invert(&lcd);
	}
}

static void ui_field_lose_focus(const ui_field_index_t field) {
	ui_field_render(field);
}

static void ui_field_gain_focus(const ui_field_index_t field) {
	ui_field_render(field);
}

static void ui_field_update_focus(const ui_field_index_t focus_field) {
	const ui_field_index_t old_field = selected_field;
	selected_field = focus_field;
	ui_field_lose_focus(old_field);
	ui_field_gain_focus(selected_field);
}

static void ui_field_navigate_up() {
	ui_field_update_focus(fields[selected_field].navigation.up);
}

static void ui_field_navigate_down() {
	ui_field_update_focus(fields[selected_field].navigation.down);
}

static void ui_field_navigate_left() {
	ui_field_update_focus(fields[selected_field].navigation.left);
}

static void ui_field_navigate_right() {
	ui_field_update_focus(fields[selected_field].navigation.right);
}

static void ui_field_value_up(const uint32_t amount) {
	ui_field_value_change_callback_t fn = fields[selected_field].value_change.up;
	if( fn != NULL ) {
		fn(amount);
	}
}

static void ui_field_value_down(const uint32_t amount) {
	ui_field_value_change_callback_t fn = fields[selected_field].value_change.down;
	if( fn != NULL ) {
		fn(amount);
	}
}

static bool ui_field_hit(const ui_field_index_t field, const uint_fast16_t x, const uint_fast16_t y) {
	if( (x >= fields[field].position.x) && (x < (fields[field].position.x + fields[field].size.w)) &&
		(y >= fields[field].position.y) && (y < (fields[field].position.y + fields[field].size.h)) ) {
		return true;
	} else {
		return false;
	}
}

static ui_field_index_t ui_fields_hit(const uint_fast16_t x, const uint_fast16_t y) {
	for(size_t i=0; i<ARRAY_SIZE(fields); i++) {
		if( ui_field_hit(i, x, y) ) {
			return i;
		}
	}
	return UI_FIELD_LAST;
}

static void ui_render_fields() {
	for(size_t i=0; i<ARRAY_SIZE(fields); i++) {
		ui_field_render(i);
	}
}

static uint32_t ui_frame = 0;

static uint32_t ui_frame_difference(const uint32_t frame1, const uint32_t frame2) {
	return frame2 - frame1;
}

static const uint32_t ui_switch_repeat_after = 30;
static const uint32_t ui_switch_repeat_rate = 10;

typedef struct ui_switch_t {
	uint32_t mask;
	uint32_t time_on;
	void (*action)(const uint32_t repeat_count);
} ui_switch_t;

void switch_increment(const uint32_t amount) {
	ui_field_value_up(amount);
}

void switch_decrement(const uint32_t amount) {
	ui_field_value_down(amount);
}

void switch_up(const uint32_t repeat_count) {
	(void)repeat_count;
	ui_field_navigate_up();
}

void switch_down(const uint32_t repeat_count) {
	(void)repeat_count;
	ui_field_navigate_down();
}

void switch_left(const uint32_t repeat_count) {
	(void)repeat_count;
	ui_field_navigate_left();
}

void switch_right(const uint32_t repeat_count) {
	(void)repeat_count;
	ui_field_navigate_right();
}

void switch_select(const uint32_t repeat_count) {
	(void)repeat_count;
}

static ui_switch_t switches[] = {
	{ .mask = SWITCH_UP,     .action = switch_up },
	{ .mask = SWITCH_DOWN,   .action = switch_down },
	{ .mask = SWITCH_LEFT,   .action = switch_left },
	{ .mask = SWITCH_RIGHT,  .action = switch_right },
	{ .mask = SWITCH_SELECT, .action = switch_select },
};

static bool handle_joysticks() {
	static uint32_t switches_history[3] = { 0, 0, 0 };
	static uint32_t switches_last = 0;

	static int32_t last_encoder_position = 0;
	const int32_t current_encoder_position = device_state->encoder_position;
	const int32_t encoder_inc = current_encoder_position - last_encoder_position;
	last_encoder_position = current_encoder_position;
	
	const uint32_t switches_raw = portapack_read_switches();
	const uint32_t switches_now = switches_raw & switches_history[0] & switches_history[1] & switches_history[2];
	switches_history[0] = switches_history[1];
	switches_history[1] = switches_history[2];
	switches_history[2] = switches_raw;

	const uint32_t switches_event = switches_now ^ switches_last;
	const uint32_t switches_event_on = switches_event & switches_now;
	switches_last = switches_now;
	bool event_occurred = false;

	if( encoder_inc > 0 ) {
		switch_increment(encoder_inc);
		event_occurred = true;
	}
	if( encoder_inc < 0 ) {
		switch_decrement(-encoder_inc);
		event_occurred = true;
	}

	for(size_t i=0; i<ARRAY_SIZE(switches); i++) {
		ui_switch_t* const sw = &switches[i];
		if( sw->action != NULL ) {
			if( switches_event_on & sw->mask ) {
				sw->time_on = ui_frame;
				sw->action(0);
				event_occurred = true;
			}

			if( switches_now & sw->mask ) {
				const uint32_t frames_since_on = ui_frame_difference(sw->time_on, ui_frame);
				if( frames_since_on >= ui_switch_repeat_after ) {
					const uint32_t frames_since_first_repeat = frames_since_on - ui_switch_repeat_after;
					if( (frames_since_first_repeat % ui_switch_repeat_rate) == 0 ) {
						const uint32_t repeat_count = frames_since_first_repeat / ui_switch_repeat_rate;
						sw->action(repeat_count);
						event_occurred = true;
					}
				}
			}
		}
	}

	return event_occurred;
}

#include "lcd_touch.h"
#if 0
static void draw_touch_data(const touch_state_t* const state, const uint_fast16_t x, const uint_fast16_t y) {
/*
	lcd_colors_invert();
	lcd_draw_string(x, y, "Touch Data", 10);
	lcd_colors_invert();
*/
/*
	int32_t X_range = max(touch_state[1].xp - touch_state[1].xn, 0);
	int32_t X = max(touch_state[1].yp - touch_state[1].xn, 0) * 1024 / X_range;

	int32_t Y_range = max(touch_state[3].yp - touch_state[3].yn, 0);
	int32_t Y = max(touch_state[3].xp - touch_state[3].yn, 0) * 1024 / Y_range;

	int32_t Z_range = max(touch_state[0].yp - touch_state[0].xn, 0);
	int32_t Z1 = max((touch_state[0].xp - touch_state[0].xn), 0) * 1024 / Z_range;
	int32_t Z2 = max((touch_state[0].yn - touch_state[0].xn), 0) * 1024 / Z_range;

	int32_t pressure = min(X * ((Z2 * 1024) / (Z1 + 1) - 1024) / 16384, 1024);

	draw_int(X, "X   %4d", x, y + 80);
	draw_int(Y, "Y   %4d", x, y + 96);
	draw_int(pressure, "P   %4d", x + 120, y + 80);
*/
/*
	if( pressure < 200 ) {
		int32_t px = (900 - X) * 240 / 800;
		int32_t py = (920 - Y) * 320 / 840;
		lcd_set_pixel(px, py);
	}
*/
	//const uint32_t k = 1;
/*
const int32_t pressure_range = touch_state[0].xp - touch_state[0].yn;
const int32_t pressure_value = touch_state[0].xn - touch_state[0].yp;
const int32_t pressure = 1024 - (pressure_value * 1024 / pressure_range);

const int32_t x_range_1 = touch_state[1].xp - touch_state[1].xn;
const int32_t x_value_1 = touch_state[1].yp + touch_state[1].yn;
const int32_t x_1 = x_value_1 * 512 / max(x_range_1, 1);

const int32_t x_range_2 = touch_state[2].xn - touch_state[2].xp;
const int32_t x_value_2 = touch_state[2].yp + touch_state[2].yn;
const int32_t x_2 = x_value_2 * 512 / max(x_range_2, 1);
*/
/*
const int32_t pressure_x = touch_state[0].xp - touch_state[0].xn;
const int32_t pressure_y = touch_state[0].yp - touch_state[0].yn;
const int32_t pressure = max(pressure_x, pressure_y);
*/
/*
const int32_t pressure = 1023 - (touch_state[0].xn - touch_state[0].yp);
const int32_t x_1 = (
	(touch_state[1].yp + touch_state[1].yn) -
	(touch_state[1].xn * 2)
) * 512 /
	(touch_state[1].xp - touch_state[1].xn)
;
*/
	draw_int(state->pressure, "P %4d", x, y + 80);
	draw_int(state->x, "X %3d", x + 7 * 8, y + 80);
	draw_int(state->y, "Y %3d", x + 13 * 8, y + 80);

	if( state->pressure ) {
		lcd_set_pixel(&lcd, state->x, state->y);
	}
/*
	draw_int(touch_state[0].xp, "X+ %4d", x, y + 16);
	draw_int(touch_state[0].xn, "X- %4d", x, y + 32);
	draw_int(touch_state[0].yp, "Y+ %4d", x, y + 48);
	draw_int(touch_state[0].yn, "Y- %4d", x, y + 64);
	
	draw_int(touch_state[1].xp, " %4d", x + 7 * 8, y + 16);
	draw_int(touch_state[1].xn, " %4d", x + 7 * 8, y + 32);
	draw_int(touch_state[1].yp, " %4d", x + 7 * 8, y + 48);
	draw_int(touch_state[1].yn, " %4d", x + 7 * 8, y + 64);

	draw_int(touch_state[2].xp, " %4d", x + 13 * 8, y + 16);
	draw_int(touch_state[2].xn, " %4d", x + 13 * 8, y + 32);
	draw_int(touch_state[2].yp, " %4d", x + 13 * 8, y + 48);
	draw_int(touch_state[2].yn, " %4d", x + 13 * 8, y + 64);
	
	draw_int(touch_state[3].xp, " %4d", x + 18 * 8, y + 16);
	draw_int(touch_state[3].xn, " %4d", x + 18 * 8, y + 32);
	draw_int(touch_state[3].yp, " %4d", x + 18 * 8, y + 48);
	draw_int(touch_state[3].yn, " %4d", x + 18 * 8, y + 64);
*/
	//const uint32_t touch_x = (touch_state[0].xp + touch_state[0].xn) / 30;
	//const uint32_t touch_y = (touch_state[2].yp + touch_state[2].yn) / 30;
}
#endif

static void draw_rtc(const uint_fast16_t x, const uint_fast16_t y) {
	draw_int(rtc_year(), "%04d/", x + 0*8, y);
	draw_int(rtc_month(), "%02d/", x + 5*8, y);
	draw_int(rtc_day_of_month(), "%02d ", x + 8*8, y);
	draw_int(rtc_hour(), "%02d:", x + 11*8, y);
	draw_int(rtc_minute(), "%02d:", x + 14*8, y);
	draw_int(rtc_second(), "%02d", x + 17*8, y);
}

static console_t console;

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

void sdio_enumerate_card_stack() {
	sdio_cclk_set_400khz();
	sdio_set_width_1bit();

	int result_cmd0 = sdio_cmd0(1);
	console_write(&console, "CMD0 ");
	sdio_draw_error(result_cmd0);

	result_cmd0 = sdio_cmd0(0);
	console_write(&console, "CMD0 ");
	sdio_draw_error(result_cmd0);

	// Interface condition
	int result_cmd8 = sdio_cmd8();
	console_write(&console, "CMD8 ");
	sdio_draw_error(result_cmd8);

	while(1) {
		int result_acmd41 = sdio_acmd41(0);
		console_write(&console, "ACMD41 ");
		sdio_draw_error(result_acmd41);
		if( result_acmd41 == SDIO_OK ) {
			break;
		}
		delay(10000000);
	}

	// result_acmd41 = sdio_acmd41((1 << 20) | (1 << 21));
	// console_write(&console, "ACMD41 ");
	// sdio_draw_error(result_acmd41);
/*
	const int result_cmd2 = sdio_cmd2();
	console_write(&console, "CMD2 ");
	sdio_draw_error(result_cmd2);

	const int result_cmd3 = sdio_cmd3();
	console_write(&console, "CMD3 ");
	sdio_draw_error(result_cmd3);
*/
//	const int result_cmd9 = sdio_cmd9();
//	console_write(&console, "CMD9 ");

blink();

/*
	const int result_cmd5 = sdio_command_no_data(SDIO_CMD5_INDEX, 0);

	if( result_cmd5 == SDIO_OK ) {
		lcd_draw_string(16*8, 4*16, "CMD5=OK", 7);
	} else {
		if( result_cmd5 == SDIO_ERROR_RESPONSE_TIMED_OUT ) {
			const int result_acmd41 = sdio_command_no_data(SDIO_ACMD41_INDEX, 0);
			if( result_acmd41 == SDIO_OK ) {
				lcd_draw_string(16*8, 4*16, "ACMD41=OK", 9);
			} else {
				if( result_acmd41 == SDIO_ERROR_RESPONSE_TIMED_OUT) {
					const int result_cmd0 = sdio_command_no_data(SDIO_CMD0_INDEX, 0);
					if( result_cmd0 == SDIO_OK ) {
						lcd_draw_string(16*8, 4*16, "CMD0=OK", 7);
					} else {
						lcd_draw_string(16*8, 4*16, "CMD0=ERR", 8);
						sdio_draw_error(result_cmd0);
					}
				} else {
					lcd_draw_string(16*8, 4*16, "ACMD41=ERR", 10);
					sdio_draw_error(result_acmd41);
				}
			}
		} else {
			lcd_draw_string(16*8, 4*16, "CMD5=ERR", 8);
			sdio_draw_error(result_cmd5);
		}
	}
*/
/*	
	if( cmd5 ) {
		card_type = SDIO;
		return;
	}

	sdio_send_command(ACMD41);
	if( acmd41 ) {
		card_type = SD;
	} else {
		card_type = MMC_OR_CEATA;
	}
*/
}

int16_t* const fft_bin = (int16_t*)0x20007100;

static void touch_started(const touch_state_t* const state) {
	const ui_field_index_t hit_index = ui_fields_hit(state->x, state->y);
	if( hit_index < UI_FIELD_LAST ) {
		ui_field_update_focus(hit_index);
	}
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

static ui_button_t numeric_entry[] = {
	{ .position = { .x =   0, .y =  16 }, .size = { .w = 80, .h = 48 }, .label = "1" },
	{ .position = { .x =  80, .y =  16 }, .size = { .w = 80, .h = 48 }, .label = "2" },
	{ .position = { .x = 160, .y =  16 }, .size = { .w = 80, .h = 48 }, .label = "3" },
	{ .position = { .x =   0, .y =  64 }, .size = { .w = 80, .h = 48 }, .label = "4" },
	{ .position = { .x =  80, .y =  64 }, .size = { .w = 80, .h = 48 }, .label = "5" },
	{ .position = { .x = 160, .y =  64 }, .size = { .w = 80, .h = 48 }, .label = "6" },
	{ .position = { .x =   0, .y = 112 }, .size = { .w = 80, .h = 48 }, .label = "7" },
	{ .position = { .x =  80, .y = 112 }, .size = { .w = 80, .h = 48 }, .label = "8" },
	{ .position = { .x = 160, .y = 112 }, .size = { .w = 80, .h = 48 }, .label = "9" },
	{ .position = { .x =  80, .y = 160 }, .size = { .w = 80, .h = 48 }, .label = "0" },
};

static void ui_button_render(const ui_button_t* const button) {
	if( button->state == UI_BUTTON_STATE_TOUCHED ) {
		lcd_colors_invert(&lcd);
	}

	lcd_draw_filled_rectangle(&lcd, button->position.x, button->position.y, button->size.w, button->size.h);
	const size_t label_length = strnlen(button->label, button->size.w / 8);
	lcd_draw_string(&lcd, button->position.x + (button->size.w - label_length * 8) / 2, button->position.y + (button->size.h - 16) / 2, button->label, label_length);

	if( button->state == UI_BUTTON_STATE_TOUCHED ) {
		lcd_colors_invert(&lcd);
	}	
}

void ui_render_numeric_entry() {
	for(size_t i=0; i<ARRAY_SIZE(numeric_entry); i++) {
		const ui_button_t* const button = &numeric_entry[i];
		ui_button_render(button);
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
		for(size_t i=0; i<ARRAY_SIZE(numeric_entry); i++) {
			ui_button_t* const button = &numeric_entry[i];
			if( ui_button_hit(button, touch->x, touch->y) ) {
				return button;
			}
		}
	}
	return NULL;
}
#if 0
static void test_lcd_coordinate_space() {
	lcd_set_background(&lcd, LCD_COLOR(  0,   0,   0));
	lcd_clear(&lcd);
	lcd_set_foreground(&lcd, LCD_COLOR(255,   0,   0));
	lcd_set_pixel(&lcd,   0,   0);
	lcd_set_foreground(&lcd, LCD_COLOR(  0, 255,   0));
	lcd_set_pixel(&lcd, 239,   0);
	lcd_set_foreground(&lcd, LCD_COLOR(  0,   0, 255));
	lcd_set_pixel(&lcd,   0, 319);
	lcd_set_foreground(&lcd, LCD_COLOR(255, 255, 255));
	lcd_set_pixel(&lcd, 239, 319);
}
#endif

#include "ipc_m0.h"
#include "ipc_m0_server.h"

#include "manchester.h"

void handle_command_packet_data_received(const void* const arg) {
	const ipc_command_packet_data_received_t* const command = arg;

	uint8_t value[5];
	uint8_t errors[5];
	manchester_decode(command->payload, value, errors, 37);

	const uint_fast8_t flag_group_1[] = {
		(value[0] >> 7) & 1,
		(value[0] >> 6) & 1,
		(value[0] >> 5) & 1,
	};
	const uint32_t id = ((((((value[0] & 0x1f) << 8) | value[1]) << 8) | value[2]) << 3) | (value[3] >> 5);
	const uint_fast8_t flag_group_2[] = {
		(value[3] >> 4) & 1,
		(value[3] >> 3) & 1,
	};
	const uint32_t mystery_value = ((value[3] & 0x7) << 5) | (value[4] >> 3);
	console_write_uint32(&console, "%1u", flag_group_1[0]);
	console_write_uint32(&console, "%1u", flag_group_1[1]);
	console_write_uint32(&console, "%1u", flag_group_1[2]);
	console_write_uint32(&console, " %8u ", id);
	console_write_uint32(&console, "%1u", flag_group_2[0]);
	console_write_uint32(&console, "%1u", flag_group_2[1]);
	console_write_uint32(&console, " %02x", mystery_value);
	console_write(&console, "/");
	for(size_t i=0; i<5; i++) {
		console_write_uint32(&console, "%02x", errors[i]);
	}
	console_writeln(&console, "");
}

int main() {
	rtc_init();
	sdio_init();
	lcd_init(&lcd);
	lcd_touch_init();

	lcd_set_background(&lcd, color_blue);
	lcd_set_foreground(&lcd, color_white);
	lcd_clear(&lcd);

	lcd_colors_invert(&lcd);
	lcd_draw_string(&lcd, 0, 0, "PortaPack                     ", 30);
	lcd_colors_invert(&lcd);

	console_init(&console, &lcd, 16 * 6, lcd.size.h - 16 * 6);
/*
	if( sdio_card_is_present() ) {
		for(size_t n=0; n<1000; n++) {
			delay(51000);
		}
		sdio_enumerate_card_stack();
	}

	blink();
*/
	ipc_command_set_audio_out_gain(&device_state->ipc_m4, 0);

	lcd_set_scroll_area(&lcd, 16 * 6, lcd.size.h);

bool numeric_entry = false;

	while(1) {
		const bool sd_card_present = sdio_card_is_present();
		lcd_draw_string(&lcd, 16*8, 5*16, sd_card_present ? "SD+" : "SD-", 3);

		lcd_colors_invert(&lcd);
		draw_rtc(11 * 8, 0);
		lcd_colors_invert(&lcd);

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
			ui_render_fields();
		}

		while( !ipc_channel_is_empty(&device_state->ipc_m4) );

		encoder_update();
		handle_joysticks();
		ipc_m0_handle();

		ipc_command_ui_frame_sync(&device_state->ipc_m4, &fft_bin[0]);
		lcd_frame_sync();

		const uint_fast16_t draw_y = lcd_scroll(&lcd, 1);
		const uint_fast16_t x = 0;
		const uint_fast16_t y = draw_y;
		lcd_start_drawing(x, y, lcd.size.w, 1);
		for(size_t i=0; i<lcd.size.w; i++) {
			const int v = max_t(uint_fast8_t, min_t(uint_fast8_t, fft_bin[i], 255), 0);
			const lcd_color_t color = spectrum_rgb3_lut[v];
			lcd_data_write_rgb(color);
		}

		ui_frame += 1;
	}

	return 0;
}
