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

#include "rx_ais.h"

#include "filters.h"
#include "decimate.h"
#include "demodulate.h"
#include "clock_recovery.h"
#include "access_code_correlator.h"
#include "packet_builder.h"

#include <cassert>

typedef struct rx_ais_state_t {
	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t bb_dec_1;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_2;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_3;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_4;
	fir_64_decim_8_cplx_s16_s16_state_t channel_dec;
	fm_demodulate_s16_s16_state_t fm_demodulate;
	clock_recovery_t clock_recovery;
	access_code_correlator_t access_code_correlator;
	packet_builder_t packet_builder;
	uint_fast8_t last_symbol;
} rx_ais_state_t;

static void rx_ais_clock_recovery_symbol_handler(const float value, void* const context) {
	rx_ais_state_t* const state = (rx_ais_state_t*)context;

	const uint_fast8_t symbol = (value >= 0.0f) ? 1 : 0;
	const uint_fast8_t nrzi_bit = (~(symbol ^ state->last_symbol)) & 1;
	state->last_symbol = symbol;
	const bool access_code_found = access_code_correlator_execute(&state->access_code_correlator, nrzi_bit);
	packet_builder_execute(&state->packet_builder, nrzi_bit, access_code_found);
}

void rx_ais_init(void* const _state, packet_builder_payload_handler_t payload_handler) {
	rx_ais_state_t* const state = (rx_ais_state_t*)_state;

	const float symbol_rate = 9600.0f;
	const float sample_rate = 19200.0f;

	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_init(&state->bb_dec_1);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_2);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_3);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_4);
	fir_64_decim_8_cplx_s16_s16_init(&state->channel_dec, taps_64_lp_031_063, 64);
	fm_demodulate_s16_s16_init(&state->fm_demodulate, sample_rate, symbol_rate / 4);
	clock_recovery_init(&state->clock_recovery, symbol_rate / sample_rate, rx_ais_clock_recovery_symbol_handler, state);
	access_code_correlator_init(&state->access_code_correlator, 0b010101010101010101010101111110, 30, 0);
	packet_builder_init(&state->packet_builder, 256, payload_handler, state);
}

void rx_ais_baseband_handler(void* const _state, complex_s8_t* const in, const size_t sample_count_in, baseband_timestamps_t* const timestamps) {
	rx_ais_state_t* const state = (rx_ais_state_t*)_state;

	size_t sample_count = sample_count_in;

	/* 2.4576MHz complex<int8>[N]
	 * -> Shift by -fs/4
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 1.2288MHz complex<int16>[N/2] */
	/* i,q: +/-128 */
	sample_count = translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16(&state->bb_dec_1, in, sample_count);
	complex_s16_t* const in_cs16 = (complex_s16_t*)in;

	/* 1.2288MHz complex<int16>[N/2]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 614.4kHz complex<int16>[N/4] */
	/* i,q: +/-1024 */
	complex_s16_t work[512];
	complex_s16_t* const work_cs16 = work;
	int16_t* const work_int16 = (int16_t*)work;
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_2, in_cs16, work_cs16, sample_count);

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
	/* i,q: +/-8192 */
	complex_s16_t* p = work_cs16;
	for(uint_fast16_t n=sample_count; n>0; n-=1) {
		p->i /= 2;
		p->q /= 2;
		p++;
	}

	/* 614.4kHz complex<int16>[N/4]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 307.2kHz complex<int16>[N/8] */
	/* i,q: +/-4096 */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_3, work_cs16, work_cs16, sample_count);

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
	/* i,q: +/-32768 */
	p = work_cs16;
	for(uint_fast16_t n=sample_count; n>0; n-=1) {
		p->i /= 8;
		p->q /= 8;
		p++;
	}

	/* 307.2kHz complex<int16>[N/8]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 153.6kHz complex<int16>[N/16] */
	/* i,q: +/-4096 */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_4, work_cs16, work_cs16, sample_count);

	timestamps->decimate_end = baseband_timestamp();

	/* 153.6kHz int16[N/16]
	 * -> FIR filter, <4.76kHz (0.031fs) pass, >9.68kHz (0.063fs) stop, gain of 1
	 * -> 19.2kHz int16[N/128] */
	sample_count = fir_64_decim_8_cplx_s16_s16(&state->channel_dec, work_cs16, work_cs16, sample_count);

	timestamps->channel_filter_end = baseband_timestamp();

	/* 19.2kHz complex<int16>[N/128]
	 * -> FM demodulation
	 * -> 19.2kHz int16[N/128] */
	fm_demodulate_s16_s16(&state->fm_demodulate, work_cs16, work_int16, sample_count);

	timestamps->demodulate_end = baseband_timestamp();

	for(size_t n=0; n<sample_count; n++) {
		clock_recovery_execute(&state->clock_recovery, work_int16[n]);		
	}
}
