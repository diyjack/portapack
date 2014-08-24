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

#include "packet_builder.h"

#include "portapack.h"

void packet_builder_init(
	packet_builder_t* const packet_builder,
	const size_t payload_length,
	packet_builder_payload_handler_t payload_handler,
	void* const context
) {
	packet_builder->payload_length = payload_length;
	packet_builder->bits_received = 0;
	packet_builder->state = PACKET_BUILDER_STATE_ACCESS_CODE_SEARCH;
	packet_builder->payload_handler = payload_handler;
	packet_builder->context = context;
	for(size_t i=0; i<ARRAY_SIZE(packet_builder->payload); i++) {
		packet_builder->payload[i] = 0;
	}
}

void packet_builder_execute(
	packet_builder_t* const packet_builder,
	const uint_fast8_t symbol,
	const bool access_code_found
) {
	switch(packet_builder->state) {
	case PACKET_BUILDER_STATE_ACCESS_CODE_SEARCH:
		if( access_code_found ) {
			packet_builder->state = PACKET_BUILDER_STATE_PAYLOAD;
			packet_builder->bits_received = 0;
		}
		break;

	case PACKET_BUILDER_STATE_PAYLOAD:
		if( packet_builder->bits_received < packet_builder->payload_length ) {
			const size_t byte_index = packet_builder->bits_received >> 3;
			const size_t bit_index = (packet_builder->bits_received & 7) ^ 7;
			const uint8_t bit = 1 << bit_index;
			if( symbol ) {
				packet_builder->payload[byte_index] |= bit;
			} else {
				packet_builder->payload[byte_index] &= ~bit;
			}
			packet_builder->bits_received += 1;
		} else {
			packet_builder->payload_handler(&packet_builder->payload, packet_builder->bits_received, packet_builder->context);
			packet_builder->state = PACKET_BUILDER_STATE_ACCESS_CODE_SEARCH;
		}
		break;

	default:
		packet_builder->state = PACKET_BUILDER_STATE_ACCESS_CODE_SEARCH;
		break;
	}
}
