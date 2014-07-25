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

#include "rx_tpms.h"

#include "i2s.h"

#include "decimate.h"
#include "demodulate.h"
#include "integrator.h"
#include "envelope.h"
#include "clock_recovery.h"
#include "access_code_correlator.h"
#include "packet_builder.h"

#include <math.h>

typedef struct rx_tpms_state_t {
	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t bb_dec_1;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_2;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_3;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_4;
	integrator_t integrator;
	envelope_t envelope;
	clock_recovery_t clock_recovery;
	access_code_correlator_t access_code_correlator;
	packet_builder_t packet_builder;
} rx_tpms_state_t;

static void rx_tpms_clock_recovery_symbol_handler(const float value, void* const context) {
	rx_tpms_state_t* const state = (rx_tpms_state_t*)context;

	const uint_fast8_t symbol = (value >= 0.0f) ? 1 : 0;
	const bool access_code_found = access_code_correlator_execute(&state->access_code_correlator, symbol);
	packet_builder_execute(&state->packet_builder, symbol, access_code_found);
}

void rx_tpms_init(void* const _state, packet_builder_payload_handler_t payload_handler) {
	rx_tpms_state_t* const state = (rx_tpms_state_t*)_state;

	const float symbol_rate = 8192.0f;
	const float sample_rate = 192000.0f;

	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_init(&state->bb_dec_1);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_2);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_3);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_4);
	integrator_init(&state->integrator, roundf(sample_rate / symbol_rate));
	envelope_init(&state->envelope, 0.08f, 0.01f);
	clock_recovery_init(&state->clock_recovery, symbol_rate / sample_rate, rx_tpms_clock_recovery_symbol_handler, state);
	access_code_correlator_init(&state->access_code_correlator, 0b01010101010101010101010101011110, 32, 2);
	packet_builder_init(&state->packet_builder, 74, payload_handler, state);
}

void rx_tpms_baseband_handler(void* const _state, complex_s8_t* const in, const size_t sample_count_in, void* const out, baseband_timestamps_t* const timestamps) {
	rx_tpms_state_t* const state = (rx_tpms_state_t*)_state;

	size_t sample_count = sample_count_in;

	timestamps->start = baseband_timestamp();

	complex_s16_t* const out_cs16 = (complex_s16_t*)out;

	/* 3.072MHz complex<int8>[N]
	 * -> Shift by -fs/4
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 1.544MHz complex<int16>[N/2] */
	/* i,q: +/-128 */
	sample_count = translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16(&state->bb_dec_1, in, sample_count);

	/* 1.544MHz complex<int16>[N/2]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 768kHz complex<int16>[N/4] */
	/* i,q: +/-1024 */
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

	/* 768kHz complex<int16>[N/4]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 384kHz complex<int16>[N/8] */
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

	/* 384kHz complex<int16>[N/8]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 192kHz complex<int16>[N/16] */
	/* i,q: +/-4096 */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_4, out, out, sample_count);

	timestamps->decimate_end = baseband_timestamp();

	timestamps->channel_filter_end = baseband_timestamp();

	/* Symbol length integrator
	 * -> integrate and dump, gain of N/32 (N = length of integrator = 23)
	 * -> 192kHz complex<int16>[N/16]
	 */
	for(uint_fast16_t i=0; i<sample_count; i++) {
		out_cs16[i] = integrator_execute(&state->integrator, out_cs16[i]);
	}

	/* 192kHz int16[N/16]
	 * -> AM demodulation
	 * -> 192kHz int16[N/16] */
	/* i,q: +/-23552 */
	float* const out_mag = (float*)out;
	am_demodulate_s16_f32(out, out_mag, sample_count);
	/* +33308 */

	for(uint_fast16_t i=0; i<sample_count; i++) {
		const float env_out = envelope_execute(&state->envelope, out_mag[i]);
		clock_recovery_execute(&state->clock_recovery, env_out);
	}

	timestamps->demodulate_end = baseband_timestamp();

	int16_t* const audio_tx_buffer = portapack_i2s_tx_empty_buffer();
	for(size_t i=0, j=0; i<I2S_BUFFER_SAMPLE_COUNT; i++, j++) {
		audio_tx_buffer[i*2] = audio_tx_buffer[i*2+1] = (int16_t)out_mag[j*4];
	}

	timestamps->audio_end = baseband_timestamp();
}
