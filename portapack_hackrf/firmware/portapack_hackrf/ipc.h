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

#include "fifo.h"

typedef FIFO<uint8_t, 8> ipc_fifo_t;

typedef struct ipc_channel_t {
	ipc_fifo_t fifo;
} ipc_channel_t;

typedef struct ipc_command_t {
	uint32_t id;
} ipc_command_t;

void ipc_channel_init(ipc_channel_t* const channel, void* const buffer);
int ipc_channel_is_empty(ipc_channel_t* const channel);
uint32_t ipc_channel_read(ipc_channel_t* const channel, void* buffer, const size_t buffer_length);
void ipc_channel_write(ipc_channel_t* const channel, const void* const buffer, const size_t buffer_length);

#endif/*__IPC_H__*/
