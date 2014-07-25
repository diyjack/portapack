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

#include "ipc.h"
#include "ipc_m0.h"

#include <string.h>

void ipc_command_packet_data_received(ipc_channel_t* const channel, const uint8_t* const payload, const size_t payload_length) {
	ipc_command_packet_data_received_t command = {
		.id = IPC_COMMAND_ID_PACKET_DATA_RECEIVED,
		.payload_length = payload_length,
	};
	memcpy(&command.payload, payload, sizeof(command.payload));
	ipc_channel_write(channel, &command, sizeof(command));
}
