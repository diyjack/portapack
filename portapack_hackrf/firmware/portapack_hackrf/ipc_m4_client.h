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

#ifndef __IPC_M4_CLIENT_H__
#define __IPC_M4_CLIENT_H__

#include <stdint.h>

#include "ipc.h"

void ipc_command_set_frequency(ipc_channel_t* const channel, const int64_t value_hz);
void ipc_command_set_rf_gain(ipc_channel_t* const channel, const int32_t value_db);
void ipc_command_set_if_gain(ipc_channel_t* const channel, const int32_t value_db);
void ipc_command_set_bb_gain(ipc_channel_t* const channel, const int32_t value_db);
void ipc_command_set_audio_out_gain(ipc_channel_t* const channel, const int32_t value_db);
void ipc_command_set_receiver_configuration(ipc_channel_t* const channel, const uint32_t index);
void ipc_command_ui_frame_sync(ipc_channel_t* const channel, int16_t* const fft_bin);

#endif/*__IPC_M4_CLIENT_H__*/