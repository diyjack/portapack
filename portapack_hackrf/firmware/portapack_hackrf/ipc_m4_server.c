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

#include <stdint.h>
#include <stdbool.h>

#include <libopencm3/lpc43xx/ipc.h>

#include <rf_path.h>
#include <max2837.h>
#include <sgpio_dma.h>

#include "portapack.h"
#include "portapack_driver.h"
#include "audio.h"

#include "ipc.h"
#include "ipc_m4.h"

static void handle_command_none(const void* const command) {
	(void)command;
}

static void handle_command_set_rf_gain(const void* const arg) {
	const ipc_command_set_rf_gain_t* const command = arg;
	const bool rf_lna_enable = (command->gain_db >= 14);
	rf_path_set_lna(rf_lna_enable);
	device_state->lna_gain_db = rf_lna_enable ? 14 : 0;
}

static void handle_command_set_if_gain(const void* const arg) {
	const ipc_command_set_if_gain_t* const command = arg;
	if( max2837_set_lna_gain(command->gain_db) ) {
		device_state->if_gain_db = command->gain_db;
	}
}

static void handle_command_set_bb_gain(const void* const arg) {
	const ipc_command_set_bb_gain_t* const command = arg;
	if( max2837_set_vga_gain(command->gain_db) ) {
		device_state->bb_gain_db = command->gain_db;
	}
}

static void handle_command_set_frequency(const void* const arg) {
	const ipc_command_set_frequency_t* const command = arg;
	if( set_frequency(command->frequency_hz) ) {
		device_state->tuned_hz = command->frequency_hz;
	}
}

static volatile bool audio_codec_initialized = false;

static void handle_command_set_audio_out_gain(const void* const arg) {
	const ipc_command_set_audio_out_gain_t* const command = arg;
	if( !audio_codec_initialized ) {
		portapack_codec_init();
		audio_codec_initialized = true;
	}
	device_state->audio_out_gain_db = portapack_audio_out_volume_set(command->gain_db);
}

static void handle_command_set_receiver_configuration(const void* const arg) {
	const ipc_command_set_receiver_configuration_t* const command = arg;
	set_rx_mode(command->index);
}

#include <math.h>
#include "complex.h"
#include "window.h"
#include "fft.h"

static void handle_command_ui_frame_sync(const void* const arg) {
	const ipc_command_ui_frame_sync_t* const command = arg;

	if( !receiver_configurations[device_state->receiver_configuration_index].enable_spectrum ) {
		return;
	}

	/* Wait for start of a new transfer. */
	const size_t last_lli_index = sgpio_dma_current_transfer_index(lli_rx, 2);
	while( sgpio_dma_current_transfer_index(lli_rx, 2) == last_lli_index );
	
	const size_t current_lli_index = sgpio_dma_current_transfer_index(lli_rx, 2);
	const size_t finished_lli_index = 1 - current_lli_index;
	complex_s8_t* const completed_buffer = lli_rx[finished_lli_index].cdestaddr;

	complex_t spectrum[256];
	/*int32_t sum_r = 0;
	int32_t sum_i = 0;
	int32_t min_r = 0;
	int32_t min_i = 0;
	int32_t max_r = 0;
	int32_t max_i = 0;*/
	const float temp_scale = 0.7071067811865476f / (256.0f);
	for(uint32_t i=0; i<256; i++) {
		const uint32_t i_rev = __RBIT(i) >> 24;

		const int32_t real = completed_buffer[i].i;
		const float real_f = (float)real;
		spectrum[i_rev].r = real_f * window[i] * temp_scale;
		
		const int32_t imag = completed_buffer[i].q;
		const float imag_f = (float)imag;
		spectrum[i_rev].i = imag_f * window[i] * temp_scale;
/*
		int32_t real = src[i].i;
		sum_r += real;
		if( real > max_r ) { max_r = real; }
		if( real < min_r ) { min_r = real; }
		spectrum[i].r = (float)real * window[i];
		
		int32_t imag = src[i].q;
		sum_i += imag;
		if( imag > max_i ) { max_i = imag; }
		if( imag < min_i ) { min_i = imag; }
		spectrum[i].i = (float)imag * window[i];
*/
	}
	
	fft_c_preswapped((float*)spectrum, 256);

	// const float spectrum_floor = -3.0f;
	// const float spectrum_gain = 30.0f;

	const float spectrum_floor = -4.5f; //-5.12f;
	const float spectrum_gain = 50.0f;

	const float log_k = 0.00000000001f; // to prevent log10f(0), which is bad...
	int16_t* const fft_bin = command->fft_bin;
	for(int_fast16_t i=-120; i<120; i++) {
		const uint_fast16_t x = i + 120;
		const uint_fast16_t bin = (i + 256) & 0xff;
		const float real = spectrum[bin].r;
		const float imag = spectrum[bin].i;
		const float bin_mag = real * real + imag * imag;
		const float bin_mag_log = log10f(bin_mag + log_k);
		// bin_mag_log should be:
		// 		-9 (bin_mag=0)
		//		-2.107210f (bin_mag=0.0078125, minimum non-zero signal)
		//		 0.150515f (bin_mag=1.414..., peak I, peak Q).
		//const int n = (int)roundf((spectrum_peak_log - bin_mag_log) * spectrum_gain + spectrum_offset_pixels);
		int n = (int)roundf((bin_mag_log - spectrum_floor) * spectrum_gain);
		fft_bin[x] = n;
		// frame->bin[x].sum += n;
		// if( n > frame->bin[x].peak ) {
		// 	frame->bin[x].peak = n;
		// }
	}
}

typedef void (*command_handler_t)(const void* const command);

static const command_handler_t command_handler[] = {
	[IPC_COMMAND_ID_NONE] = handle_command_none,
	[IPC_COMMAND_ID_SET_RF_GAIN] = handle_command_set_rf_gain,
	[IPC_COMMAND_ID_SET_IF_GAIN] = handle_command_set_if_gain,
	[IPC_COMMAND_ID_SET_BB_GAIN] = handle_command_set_bb_gain,
	[IPC_COMMAND_ID_SET_FREQUENCY] = handle_command_set_frequency,
	[IPC_COMMAND_ID_SET_AUDIO_OUT_GAIN] = handle_command_set_audio_out_gain,
	[IPC_COMMAND_ID_SET_RECEIVER_CONFIGURATION] = handle_command_set_receiver_configuration,
	[IPC_COMMAND_ID_UI_FRAME_SYNC] = handle_command_ui_frame_sync,
};
static const size_t command_handler_count = sizeof(command_handler) / sizeof(command_handler[0]);

void m0core_isr() {
	ipc_m0apptxevent_clear();

	while( !ipc_channel_is_empty(&device_state->ipc_m4) ) {
		uint8_t command_buffer[256];
		const ipc_command_id_t command_id = ipc_channel_read(&device_state->ipc_m4, command_buffer, sizeof(command_buffer));
		if( command_id < command_handler_count) {
			command_handler[command_id](command_buffer);
		}
	}
}
