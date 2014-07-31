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

#ifndef __IPC_M0_CLIENT_H__
#define __IPC_M0_CLIENT_H__

#include <stdint.h>
#include <stddef.h>

#include "ipc.h"

void ipc_command_packet_data_received(ipc_channel_t* const channel, const uint8_t* const payload, const size_t payload_length);
void ipc_command_spectrum_data(ipc_channel_t* const channel, uint8_t* const avg, uint8_t* const peak, const size_t bins);
void ipc_command_rtc_second(ipc_channel_t* const channel);

#endif/*__IPC_M0_CLIENT_H__*/
