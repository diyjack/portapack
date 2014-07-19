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

#ifndef __IPC_H__
#define __IPC_H__

#include <stdint.h>
#include <stddef.h>

#include "kfifo.h"

typedef struct __STRUCT_KFIFO_PTR(unsigned char, 1, void) ipc_fifo_t;

typedef struct ipc_channel_t {
	ipc_fifo_t fifo;
} ipc_channel_t;

typedef enum {
	IPC_COMMAND_ID_NONE = 0,
	IPC_COMMAND_ID_SET_FREQUENCY = 1,
	IPC_COMMAND_ID_SET_RF_GAIN = 2,
	IPC_COMMAND_ID_SET_IF_GAIN = 3,
	IPC_COMMAND_ID_SET_BB_GAIN = 4,
	IPC_COMMAND_ID_SET_AUDIO_OUT_GAIN = 5,
	IPC_COMMAND_ID_SET_RECEIVER_CONFIGURATION = 6,
	IPC_COMMAND_ID_UI_FRAME_SYNC = 7,
} ipc_command_id_t;

typedef struct ipc_command_t {
	uint32_t id;
} ipc_command_t;

typedef struct ipc_command_set_frequency_t {
	uint32_t id;
	int64_t frequency_hz;
} ipc_command_set_frequency_t;

typedef struct ipc_command_set_rf_gain_t {
	uint32_t id;
	int32_t gain_db;
} ipc_command_set_rf_gain_t;

typedef struct ipc_command_set_if_gain_t {
	uint32_t id;
	int32_t gain_db;
} ipc_command_set_if_gain_t;

typedef struct ipc_command_set_bb_gain_t {
	uint32_t id;
	int32_t gain_db;
} ipc_command_set_bb_gain_t;

typedef struct ipc_command_set_audio_out_gain_t {
	uint32_t id;
	int32_t gain_db;
} ipc_command_set_audio_out_gain_t;

typedef struct ipc_command_set_receiver_configuration_t {
	uint32_t id;
	uint32_t index;
} ipc_command_set_receiver_configuration_t;

typedef struct ipc_command_ui_frame_sync_t {
	uint32_t id;
	int16_t* fft_bin;
} ipc_command_ui_frame_sync_t;

void ipc_channel_init(ipc_channel_t* const channel, void* const buffer, const size_t buffer_size);

void ipc_command_set_frequency(ipc_channel_t* const channel, const int64_t value_hz);
void ipc_command_set_rf_gain(ipc_channel_t* const channel, const int32_t value_db);
void ipc_command_set_if_gain(ipc_channel_t* const channel, const int32_t value_db);
void ipc_command_set_bb_gain(ipc_channel_t* const channel, const int32_t value_db);
void ipc_command_set_audio_out_gain(ipc_channel_t* const channel, const int32_t value_db);
void ipc_command_set_receiver_configuration(ipc_channel_t* const channel, const uint32_t index);
void ipc_command_ui_frame_sync(ipc_channel_t* const channel, int16_t* const fft_bin);

int ipc_channel_is_empty(ipc_channel_t* const channel);
ipc_command_id_t ipc_channel_read(ipc_channel_t* const channel, void* buffer, const size_t buffer_length);

#endif/*__IPC_H__*/
