/*
 * Copyright (C) 2013 Jared Boone, ShareBrained Technology, Inc.
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

#ifndef __DECIMATE_H__
#define __DECIMATE_H__

#include <stdint.h>

#include "complex.h"

typedef struct translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t {
	uint32_t q1_i0;
	uint32_t q0_i1;
} translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t;

void translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_init(translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t* const state);
void translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16(
	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t* const state,
	complex_s8_t* const start,
	complex_s8_t* const end
);

typedef struct decimate_by_2_s8_s16_state_t {
	uint32_t i;
	uint32_t q;
} decimate_by_2_s8_s16_state_t;

void decimate_by_2_s8_s16_init(decimate_by_2_s8_s16_state_t* const state);
void decimate_by_2_s8_s16(decimate_by_2_s8_s16_state_t* const state, void* const start, void* const end);

typedef struct decimate_by_2_s16_s32_state_t {
	uint32_t iq0;
	uint32_t iq1;
} decimate_by_2_s16_s32_state_t;

void decimate_by_2_s16_s32_init(decimate_by_2_s16_s32_state_t* const state);
void decimate_by_2_s16_s32(
	decimate_by_2_s16_s32_state_t* const state,
	complex_s16_t* const start,
	complex_s16_t* const end
);

typedef struct decimate_by_2_s16_s16_state_t {
	uint32_t iq0;
	uint32_t iq1;
} decimate_by_2_s16_s16_state_t;

void decimate_by_2_s16_s16_init(decimate_by_2_s16_s16_state_t* const state);
void decimate_by_2_s16_s16(
	decimate_by_2_s16_s16_state_t* const state,
	complex_s16_t* const src,
	complex_s16_t* const dst,
	int32_t n
);

#endif/*__DECIMATE_H__*/
