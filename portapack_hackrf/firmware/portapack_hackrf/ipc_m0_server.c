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

#include <libopencm3/lpc43xx/ipc.h>

#include "portapack_driver.h"

#include "ipc.h"
#include "ipc_m0.h"
#include "ipc_m0_server.h"

static void handle_command_none(const void* const command) {
	(void)command;
}

typedef void (*command_handler_t)(const void* const command);

static const command_handler_t command_handler[] = {
	[IPC_COMMAND_ID_NONE] = handle_command_none,
	[IPC_COMMAND_ID_PACKET_DATA_RECEIVED] = handle_command_packet_data_received,
	[IPC_COMMAND_ID_SPECTRUM_DATA] = handle_command_spectrum_data,
	[IPC_COMMAND_ID_RTC_SECOND] = handle_command_rtc_second,
};
static const size_t command_handler_count = sizeof(command_handler) / sizeof(command_handler[0]);

/* TODO: Move to libopencm3 */
#include <libopencm3/lpc43xx/creg.h>

static void ipc_m4txevent_clear(void) {
	CREG_M4TXEVENT &= ~CREG_M4TXEVENT_TXEVCLR;
}

void ipc_m0_handle() {
	while( !ipc_channel_is_empty(&device_state->ipc_m0) ) {
		uint8_t command_buffer[256];
		const ipc_command_id_t command_id = ipc_channel_read(&device_state->ipc_m0, command_buffer, sizeof(command_buffer));
		if( command_id < command_handler_count) {
			command_handler[command_id](command_buffer);
		}
	}
}

void m4core_isr() {
	ipc_m4txevent_clear();
	ipc_m0_handle();
}
