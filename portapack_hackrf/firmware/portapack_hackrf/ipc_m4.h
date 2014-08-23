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

#ifndef __IPC_M4_H__
#define __IPC_M4_H__

#include <stdint.h>

#include "ipc.h"

/* TODO: In C++, reimplement with map of some sort */
typedef enum {
	IPC_COMMAND_ID_NONE = 0,
	IPC_COMMAND_ID_SET_FREQUENCY = 1,
	IPC_COMMAND_ID_SET_RF_GAIN = 2,
	IPC_COMMAND_ID_SET_IF_GAIN = 3,
	IPC_COMMAND_ID_SET_BB_GAIN = 4,
	IPC_COMMAND_ID_SET_AUDIO_OUT_GAIN = 5,
	IPC_COMMAND_ID_SET_RECEIVER_CONFIGURATION = 6,
	IPC_COMMAND_ID_SPECTRUM_DATA_DONE = 7,
} ipc_command_id_t;

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

typedef struct ipc_command_spectrum_data_done_t {
	uint32_t id;
} ipc_command_spectrum_data_done_t;

#endif/*__IPC_M4_H__*/
