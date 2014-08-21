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

#include "rx_tpms_fsk.h"

#include "i2s.h"

#include "decimate.h"
#include "clock_recovery.h"
#include "access_code_correlator.h"
#include "packet_builder.h"

#include <math.h>

typedef struct rx_tpms_fsk_state_t {
	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t bb_dec_1;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_2;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_3;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_4;
	complex_s16_t symbol_z[10];
	clock_recovery_t clock_recovery;
	access_code_correlator_t access_code_correlator;
	packet_builder_t packet_builder;
} rx_tpms_fsk_state_t;

static void rx_tpms_fsk_clock_recovery_symbol_handler(const float value, void* const context) {
	rx_tpms_fsk_state_t* const state = (rx_tpms_fsk_state_t*)context;

	const uint_fast8_t symbol = (value >= 0.0f) ? 1 : 0;
	const bool access_code_found = access_code_correlator_execute(&state->access_code_correlator, symbol);
	packet_builder_execute(&state->packet_builder, symbol, access_code_found);
}

void rx_tpms_fsk_init(void* const _state, packet_builder_payload_handler_t payload_handler) {
	rx_tpms_fsk_state_t* const state = (rx_tpms_fsk_state_t*)_state;

	const float symbol_rate = 19200.0f;
	const float sample_rate = 76800.0f;

	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_init(&state->bb_dec_1);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_2);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_3);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_4);
	for(size_t i=0; i<ARRAY_SIZE(state->symbol_z); i++) {
		state->symbol_z[i].i = 0;
		state->symbol_z[i].q = 0;
	}
	clock_recovery_init(&state->clock_recovery, symbol_rate / sample_rate, rx_tpms_fsk_clock_recovery_symbol_handler, state);
	access_code_correlator_init(&state->access_code_correlator, 0b01010101010101010101010101010110, 32, 2);
	packet_builder_init(&state->packet_builder, 160, payload_handler, state);
}

void rx_tpms_fsk_baseband_handler(void* const _state, complex_s8_t* const in, const size_t sample_count_in, baseband_timestamps_t* const timestamps) {
	rx_tpms_fsk_state_t* const state = (rx_tpms_fsk_state_t*)_state;

	size_t sample_count = sample_count_in;

	/* 2.4576MHz complex<int8>[N]
	 * -> Shift by -fs/4
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 1.2288MHz complex<int16>[N/2] */
	/* i,q: +/-128 */
	sample_count = translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16(&state->bb_dec_1, in, sample_count);

	/* 1.2288MHz complex<int16>[N/2]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 614.4kHz complex<int16>[N/4] */
	/* i,q: +/-1024 */
	complex_s16_t work[512];
	void* const out = work;
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_2, (complex_s16_t*)in, out, sample_count);

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
	/* i,q: +/-8192 */
	complex_s16_t* p;
	p = (complex_s16_t*)out;
	for(uint_fast16_t n=sample_count; n>0; n-=1) {
		p->i /= 2;
		p->q /= 2;
		p++;
	}

	/* 614.4kHz complex<int16>[N/4]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 307.2kHz complex<int16>[N/8] */
	/* i,q: +/-4096 */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_3, out, out, sample_count);

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
	/* i,q: +/-32768 */
	p = (complex_s16_t*)out;
	for(uint_fast16_t n=sample_count; n>0; n-=1) {
		p->i /= 8;
		p->q /= 8;
		p++;
	}

	/* 307.2kHz complex<int16>[N/8]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 153.6kHz complex<int16>[N/16] */
	/* i,q: +/-4096 */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_4, out, out, sample_count);

	timestamps->decimate_end = baseband_timestamp();

	timestamps->channel_filter_end = baseband_timestamp();

	const complex_s16_t* const c = (complex_s16_t*)out;
	const int16_t t0 = -3, t6 = -3;
	const int16_t t2 = 19, t4 = 19;
	const int16_t t3 = 32;
	int16_t* const audio_tx_buffer = portapack_i2s_tx_empty_buffer();
	for(size_t i=0; i<sample_count; i+=4) {
		state->symbol_z[0] = state->symbol_z[4];
		state->symbol_z[1] = state->symbol_z[5];
		state->symbol_z[2] = state->symbol_z[6];
		state->symbol_z[3] = state->symbol_z[7];
		state->symbol_z[4] = state->symbol_z[8];
		state->symbol_z[5] = state->symbol_z[9];

		state->symbol_z[6] = c[i+0];
		state->symbol_z[7] = c[i+1];
		state->symbol_z[8] = c[i+2];
		state->symbol_z[9] = c[i+3];

		int32_t i0 = state->symbol_z[0].i * t0;
		int32_t q0 = state->symbol_z[0].q * t0;
		int32_t i1 = -state->symbol_z[2].i * t0;
 		int32_t q1 = -state->symbol_z[2].q * t0;
		i0 -= state->symbol_z[2].i * t2;
		q0 -= state->symbol_z[2].q * t2;
 		i1 += state->symbol_z[4].i * t2;
		q1 += state->symbol_z[4].q * t2;
		i0 += state->symbol_z[4].i * t4;
		q0 += state->symbol_z[4].q * t4;
		i1 -= state->symbol_z[6].i * t4;
		q1 -= state->symbol_z[6].q * t4;
		i0 -= state->symbol_z[6].i * t6;
		q0 -= state->symbol_z[6].q * t6;
		i1 += state->symbol_z[8].i * t6;
		q1 += state->symbol_z[8].q * t6;
		int32_t h_i0 = i0 - state->symbol_z[3].q * t3;
		int32_t h_q0 = q0 + state->symbol_z[3].i * t3;
		int32_t l_i0 = i0 + state->symbol_z[3].q * t3;
		int32_t l_q0 = q0 - state->symbol_z[3].i * t3;

		h_i0 /= 64;
		h_q0 /= 64;
	 	l_i0 /= 64;
	 	l_q0 /= 64;

	 	const float h0_mag = sqrtf(h_i0 * h_i0 + h_q0 * h_q0);
	 	const float l0_mag = sqrtf(l_i0 * l_i0 + l_q0 * l_q0);
	 	const float diff0 = h0_mag - l0_mag;

		int32_t h_i1 = i1 + state->symbol_z[5].q * t3;
		int32_t h_q1 = q1 - state->symbol_z[5].i * t3;
		int32_t l_i1 = i1 - state->symbol_z[5].q * t3;
		int32_t l_q1 = q1 + state->symbol_z[5].i * t3;

		h_i1 /= 64;
		h_q1 /= 64;
	 	l_i1 /= 64;
	 	l_q1 /= 64;
	 	
	 	const float h1_mag = sqrtf(h_i1 * h_i1 + h_q1 * h_q1);
	 	const float l1_mag = sqrtf(l_i1 * l_i1 + l_q1 * l_q1);
	 	const float diff1 = h1_mag - l1_mag;

	 	audio_tx_buffer[(i>>2)*2+0] = audio_tx_buffer[(i>>2)*2+1] = diff0;

		clock_recovery_execute(&state->clock_recovery, diff0);
		clock_recovery_execute(&state->clock_recovery, diff1);
	}

	timestamps->demodulate_end = baseband_timestamp();
}
