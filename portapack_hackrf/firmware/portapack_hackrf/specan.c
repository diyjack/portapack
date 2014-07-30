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

#include "specan.h"

#include "complex.h"
#include "window.h"
#include "fft.h"

#include <math.h>

typedef struct specan_state_t {
	int16_t fft_bin[256];
} specan_state_t;

void specan_init(void* const _state) {
	specan_state_t* const state = (specan_state_t*)_state;

	for(size_t i=0; i<ARRAY_SIZE(state->fft_bin); i++) {
		state->fft_bin[i] = 0;
	}
}

void specan_baseband_handler(void* const _state, complex_s8_t* const in, const size_t sample_count_in, baseband_timestamps_t* const timestamps) {
	specan_state_t* const state = (specan_state_t*)_state;
	(void)sample_count_in;
	(void)timestamps;

	complex_t spectrum[256];
	/*int32_t sum_r = 0;
	int32_t sum_i = 0;
	int32_t min_r = 0;
	int32_t min_i = 0;
	int32_t max_r = 0;
	int32_t max_i = 0;*/
	const float temp_scale = 0.7071067811865476f / (256.0f);
	for(uint32_t i=0; i<256; i++) {
		const uint32_t i_rev = __RBIT(i) >> 24;

		const int32_t real = in[i].i;
		const float real_f = (float)real;
		spectrum[i_rev].r = real_f * window[i] * temp_scale;
		
		const int32_t imag = in[i].q;
		const float imag_f = (float)imag;
		spectrum[i_rev].i = imag_f * window[i] * temp_scale;
/*
		int32_t real = src[i].i;
		sum_r += real;
		if( real > max_r ) { max_r = real; }
		if( real < min_r ) { min_r = real; }
		spectrum[i].r = (float)real * window[i];
		
		int32_t imag = src[i].q;
		sum_i += imag;
		if( imag > max_i ) { max_i = imag; }
		if( imag < min_i ) { min_i = imag; }
		spectrum[i].i = (float)imag * window[i];
*/
	}
	
	fft_c_preswapped((float*)spectrum, 256);

	// const float spectrum_floor = -3.0f;
	// const float spectrum_gain = 30.0f;

	const float spectrum_floor = -4.5f; //-5.12f;
	const float spectrum_gain = 50.0f;

	const float log_k = 0.00000000001f; // to prevent log10f(0), which is bad...
	for(int_fast16_t i=-120; i<120; i++) {
		const uint_fast16_t x = i + 120;
		const uint_fast16_t bin = (i + 256) & 0xff;
		const float real = spectrum[bin].r;
		const float imag = spectrum[bin].i;
		const float bin_mag = real * real + imag * imag;
		const float bin_mag_log = log10f(bin_mag + log_k);
		// bin_mag_log should be:
		// 		-9 (bin_mag=0)
		//		-2.107210f (bin_mag=0.0078125, minimum non-zero signal)
		//		 0.150515f (bin_mag=1.414..., peak I, peak Q).
		//const int n = (int)roundf((spectrum_peak_log - bin_mag_log) * spectrum_gain + spectrum_offset_pixels);
		int n = (int)roundf((bin_mag_log - spectrum_floor) * spectrum_gain);
		state->fft_bin[x] = n;
		// frame->bin[x].sum += n;
		// if( n > frame->bin[x].peak ) {
		// 	frame->bin[x].peak = n;
		// }
	}
}

