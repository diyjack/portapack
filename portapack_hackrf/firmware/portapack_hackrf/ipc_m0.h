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

#ifndef __IPC_M0_H__
#define __IPC_M0_H__

typedef enum {
	IPC_COMMAND_ID_NONE = 0,
	IPC_COMMAND_ID_PACKET_DATA_RECEIVED = 1,
	IPC_COMMAND_ID_SPECTRUM_DATA = 2,
} ipc_command_id_t;

typedef struct ipc_command_packet_data_received_t {
	uint32_t id;
	size_t payload_length;
	uint8_t payload[32];
} ipc_command_packet_data_received_t;

typedef struct ipc_command_spectrum_data_t {
	uint32_t id;
	uint8_t* avg;
	uint8_t* peak;
	size_t bins;
} ipc_command_spectrum_data_t;

#endif/*__IPC_M0_H__*/
