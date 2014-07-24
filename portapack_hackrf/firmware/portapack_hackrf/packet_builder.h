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

#ifndef __PACKET_BUILDER_H__
#define __PACKET_BUILDER_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
	PACKET_BUILDER_STATE_ACCESS_CODE_SEARCH = 0,
	PACKET_BUILDER_STATE_PAYLOAD = 1,
} packet_builder_state_t;

typedef void (*packet_builder_payload_handler_t)(const void* const payload, const size_t payload_length, void* const context);

typedef struct packet_builder_t {
	size_t payload_length;
	size_t bits_received;
	packet_builder_state_t state;
	packet_builder_payload_handler_t payload_handler;
	void* context;
	uint32_t payload[8];
} packet_builder_t;

void packet_builder_init(
	packet_builder_t* const packet_builder,
	const size_t payload_length,
	packet_builder_payload_handler_t payload_handler,
	void* const context
);

void packet_builder_execute(
	packet_builder_t* const packet_builder,
	const uint_fast8_t symbol,
	const bool access_code_found
);

#endif/*__PACKET_BUILDER_H__*/
