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

int ipc_channel_is_empty(ipc_channel_t* const channel) {
	return kfifo_is_empty(&channel->fifo);
}

uint32_t ipc_channel_read(ipc_channel_t* const channel, void* buffer, const size_t buffer_length) {
	if( kfifo_out(&channel->fifo, buffer, buffer_length) > 0 ) {
		const ipc_command_t* const command = (ipc_command_t*)buffer;
		return command->id;
	} else {
		return 0;
	}
}

void ipc_channel_write(ipc_channel_t* const channel, const void* const buffer, const size_t buffer_length) {
	kfifo_in(&channel->fifo, buffer, buffer_length);
	__SEV();
}
