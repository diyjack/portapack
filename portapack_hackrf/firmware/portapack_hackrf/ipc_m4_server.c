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
#include "ipc_m4_server.h"

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

typedef void (*command_handler_t)(const void* const command);

static const command_handler_t command_handler[] = {
	[IPC_COMMAND_ID_NONE] = handle_command_none,
	[IPC_COMMAND_ID_SET_RF_GAIN] = handle_command_set_rf_gain,
	[IPC_COMMAND_ID_SET_IF_GAIN] = handle_command_set_if_gain,
	[IPC_COMMAND_ID_SET_BB_GAIN] = handle_command_set_bb_gain,
	[IPC_COMMAND_ID_SET_FREQUENCY] = handle_command_set_frequency,
	[IPC_COMMAND_ID_SET_AUDIO_OUT_GAIN] = handle_command_set_audio_out_gain,
	[IPC_COMMAND_ID_SET_RECEIVER_CONFIGURATION] = handle_command_set_receiver_configuration,
	[IPC_COMMAND_ID_SPECTRUM_DATA_DONE] = handle_command_spectrum_data_done,
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
