/*
 * Copyright (C) 2014 Jared Boone <jared@sharebrained.com>
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

#include "ipc.h"

#include <stdint.h>

#include "arm_intrinsics.h"

void ipc_channel_init(ipc_channel_t* const channel, void* const buffer, const size_t buffer_size) {
	kfifo_init(&channel->fifo, (unsigned char*)buffer, buffer_size);
}

void ipc_command_set_frequency(ipc_channel_t* const channel, const int64_t value_hz) {
	ipc_command_set_frequency_t command = {
		.id = IPC_COMMAND_ID_SET_FREQUENCY,
		.frequency_hz = value_hz
	};
	kfifo_in(&channel->fifo, &command, sizeof(command));
	__SEV();
}

void ipc_command_set_rf_gain(ipc_channel_t* const channel, const int32_t value_db) {
	ipc_command_set_rf_gain_t command = {
		.id = IPC_COMMAND_ID_SET_RF_GAIN,
		.gain_db = value_db
	};
	kfifo_in(&channel->fifo, &command, sizeof(command));
	__SEV();
}

void ipc_command_set_if_gain(ipc_channel_t* const channel, const int32_t value_db) {
	ipc_command_set_if_gain_t command = {
		.id = IPC_COMMAND_ID_SET_IF_GAIN,
		.gain_db = value_db
	};
	kfifo_in(&channel->fifo, &command, sizeof(command));
	__SEV();
}

void ipc_command_set_bb_gain(ipc_channel_t* const channel, const int32_t value_db) {
	ipc_command_set_bb_gain_t command = {
		.id = IPC_COMMAND_ID_SET_BB_GAIN,
		.gain_db = value_db
	};
	kfifo_in(&channel->fifo, &command, sizeof(command));
	__SEV();
}

void ipc_command_set_audio_out_gain(ipc_channel_t* const channel, const int32_t value_db) {
	ipc_command_set_audio_out_gain_t command = {
		.id = IPC_COMMAND_ID_SET_AUDIO_OUT_GAIN,
		.gain_db = value_db
	};
	kfifo_in(&channel->fifo, &command, sizeof(command));
	__SEV();
}

void ipc_command_set_receiver_configuration(ipc_channel_t* const channel, const uint32_t index) {
	ipc_command_set_receiver_configuration_t command = {
		.id = IPC_COMMAND_ID_SET_RECEIVER_CONFIGURATION,
		.index = index
	};
	kfifo_in(&channel->fifo, &command, sizeof(command));
	__SEV();
}

void ipc_command_ui_frame_sync(ipc_channel_t* const channel, int16_t* const fft_bin) {
	ipc_command_ui_frame_sync_t command = {
		.id = IPC_COMMAND_ID_UI_FRAME_SYNC,
		.fft_bin = fft_bin,
	};
	kfifo_in(&channel->fifo, &command, sizeof(command));
	__SEV();
}

int ipc_channel_is_empty(ipc_channel_t* const channel) {
	return kfifo_is_empty(&channel->fifo);
}

ipc_command_id_t ipc_channel_read(ipc_channel_t* const channel, void* buffer, const size_t buffer_length) {
	if( kfifo_out(&channel->fifo, buffer, buffer_length) > 0 ) {
		const ipc_command_t* const command = (ipc_command_t*)buffer;
		return (ipc_command_id_t)command->id;
	} else {
		return IPC_COMMAND_ID_NONE;
	}
}
