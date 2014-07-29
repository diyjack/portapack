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

#include "portapack.h"

#include <libopencm3/cm3/vector.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/lpc43xx/m4/nvic.h>

#include <hackrf_core.h>
#include <rf_path.h>
#include <sgpio.h>
#include <streaming.h>
#include <max2837.h>
#include <gpdma.h>
#include <sgpio_dma.h>

#include "portapack_driver.h"
#include "audio.h"
#include "cpld.h"
#include "i2s.h"
#include "m0_startup.h"

#include "lcd.h"
#include "tuning.h"

#include "complex.h"
#include "decimate.h"
#include "demodulate.h"

#include "rx_fm_broadcast.h"
#include "rx_tpms_ask.h"
#include "rx_tpms_fsk.h"

#include "ipc.h"
#include "ipc_m4.h"
#include "ipc_m0_client.h"

#include "linux_stuff.h"

gpdma_lli_t lli_rx[2];

uint32_t baseband_timestamp() {
	return systick_get_value();
}

static uint32_t systick_difference(const uint32_t t1, const uint32_t t2) {
	return (t1 - t2) & 0xffffff;
}

void copy_to_audio_output(const int16_t* const source, const size_t sample_count) {
	if( sample_count != I2S_BUFFER_SAMPLE_COUNT ) {
		return;
	}

	int16_t* const audio_tx_buffer = portapack_i2s_tx_empty_buffer();
	for(size_t i=0, j=0; i<I2S_BUFFER_SAMPLE_COUNT; i++, j++) {
		audio_tx_buffer[i*2] = audio_tx_buffer[i*2+1] = source[j];
	}
}

/* Narrowband audio filter */
/* 96kHz int16_t input
 * -> FIR filter, <3kHz (0.031fs) pass, >6kHz (0.063fs) stop
 * -> 48kHz int16_t output, gain of 1.0 (I think).
 * Padded to multiple of four taps for unrolled FIR code.
 */
/* TODO: Review this filter, it's very quick and dirty. */
static const int16_t taps_64_lp_031_063[] = {
	  -254,    255,    244,    269,    302,    325,    325,    290,
	   215,     99,    -56,   -241,   -442,   -643,   -820,   -950,
	 -1009,   -974,   -828,   -558,   -160,    361,    992,   1707,
	  2477,   3264,   4027,   4723,   5312,   5761,   6042,   6203,
	  6042,   5761,   5312,   4723,   4027,   3264,   2477,   1707,
	   992,    361,   -160,   -558,   -828,   -974,  -1009,   -950,
	  -820,   -643,   -442,   -241,    -56,     99,    215,    290,
	   325,    325,    302,    269,    244,    255,   -254,      0,
};

static void rx_tpms_ask_packet_handler(const void* const payload, const size_t payload_length, void* const context) {
	(void)context;
	ipc_command_packet_data_received(&device_state->ipc_m0, payload, payload_length);
}

static void rx_tpms_ask_init_wrapper(void* const _state) {
	rx_tpms_ask_init(_state, rx_tpms_ask_packet_handler);
}

static void rx_tpms_fsk_packet_handler(const void* const payload, const size_t payload_length, void* const context) {
	(void)context;
	ipc_command_packet_data_received(&device_state->ipc_m0, payload, payload_length);
}

static void rx_tpms_fsk_init_wrapper(void* const _state) {
	rx_tpms_fsk_init(_state, rx_tpms_fsk_packet_handler);
}
#if 0
typedef void (*init_fn)(void* state);
typedef size_t (*process_fn)(void* state, void* in, size_t in_count, void* out);

typedef struct process_block_t {
	init_fn* init;
	process_fn* process;
} process_block_t;

typedef struct receiver_handler_chain_t {
	channel_decimator_t* channel_decimator;
	demodulator_t* demodulator;
	decoder_t* decoder;
	output_t* output;
}

typedef channel_decimator_t* 

void translate_by_fs_over_4_and_decimate_by_4_to_0_065fs() {
	/* Translate spectrum down by fs/4 (fs/4 becomes 0Hz)
	 * Decimate by 2 with a 3rd-order CIC filter.
	 * Decimate by 2 with a 3rd-order CIC filter.
	 */
	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16(&state->dec_stage_1_state, in, sample_count_in);
	decimate_by_2_s16_s16(&state->dec_stage_2_state, (complex_s16_t*)in, out, sample_count_in / 2);
}
#endif

typedef struct rx_fm_narrowband_to_audio_state_t {
	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t bb_dec_1;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_2;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_3;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_4;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_5;
	// TODO: Channel filter here.
	fm_demodulate_s16_s16_state_t fm_demodulate;
	fir_64_decim_2_real_s16_s16_state_t audio_dec;
} rx_fm_narrowband_to_audio_state_t;

void rx_fm_narrowband_to_audio_init(void* const _state) {
	rx_fm_narrowband_to_audio_state_t* const state = (rx_fm_narrowband_to_audio_state_t*)_state;

	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_init(&state->bb_dec_1);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_2);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_3);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_4);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_5);
	// TODO: Channel filter here.
	fm_demodulate_s16_s16_init(&state->fm_demodulate, 96000, 2500);
	fir_64_decim_2_real_s16_s16_init(&state->audio_dec, taps_64_lp_031_063, 64);
}

void rx_fm_narrowband_to_audio_baseband_handler(void* const _state, complex_s8_t* const in, const size_t sample_count_in, void* const out, baseband_timestamps_t* const timestamps) {
	rx_fm_narrowband_to_audio_state_t* const state = (rx_fm_narrowband_to_audio_state_t*)_state;

	size_t sample_count = sample_count_in;

	timestamps->start = baseband_timestamp();

	/* 3.072MHz complex<int8>[N]
	 * -> Shift by -fs/4
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 1.544MHz complex<int16>[N/2] */
	sample_count = translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16(&state->bb_dec_1, in, sample_count);

	/* 1.544MHz complex<int16>[N/2]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 768kHz complex<int16>[N/4] */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_2, (complex_s16_t*)in, out, sample_count);

	/* TODO: Gain through five CICs will be 32768 (8 ^ 5). Incorporate gain adjustment
	 * somewhere in the chain.
	 */

	/* TODO: Create a decimate_by_2_s16_s16 with gain adjustment and rounding, maybe use
	 * SSAT (no rounding) or use SMMULR/SMMLAR/SMMLSR (provides rounding)?
	 */

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
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
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_3, out, out, sample_count);

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
	p = (complex_s16_t*)out;
	for(uint_fast16_t n=sample_count; n>0; n-=1) {
		p->i /= 8;
		p->q /= 8;
		p++;
	}

	/* 384kHz complex<int16>[N/8]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 192kHz complex<int16>[N/16] */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_4, out, out, sample_count);

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
	p = (complex_s16_t*)out;
	for(uint_fast16_t n=sample_count; n>0; n-=1) {
		p->i /= 8;
		p->q /= 8;
		p++;
	}

	/* 192kHz complex<int16>[N/16]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 96kHz complex<int16>[N/32] */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_5, out, out, sample_count);

	timestamps->decimate_end = baseband_timestamp();

	// TODO: Design a proper channel filter.

	timestamps->channel_filter_end = baseband_timestamp();

	/* 96kHz complex<int16>[N/32]
	 * -> FM demodulation
	 * -> 96kHz int16[N/32] */
	fm_demodulate_s16_s16(&state->fm_demodulate, out, out, sample_count);

	timestamps->demodulate_end = baseband_timestamp();

	/* 96kHz int16[N/32]
	 * -> FIR filter, <3kHz (0.031fs) pass, >6kHz (0.063fs) stop, gain of 1
	 * -> 48kHz int16[N/64] */
	sample_count = fir_64_decim_2_real_s16_s16(&state->audio_dec, out, out, sample_count);

	timestamps->audio_end = baseband_timestamp();

	copy_to_audio_output(out, sample_count);
}

typedef struct rx_am_to_audio_state_t {
	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_state_t bb_dec_1;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_2;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_3;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_4;
	fir_cic3_decim_2_s16_s16_state_t bb_dec_5;
	// TODO: Channel filter here.
	// TODO: Rename NBFM filter to be more generic, so it can be shared with AM, others.
	fir_64_decim_2_real_s16_s16_state_t audio_dec;
} rx_am_to_audio_state_t;

void rx_am_to_audio_init(void* const _state) {
	rx_am_to_audio_state_t* const state = (rx_am_to_audio_state_t*)_state;
	translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16_init(&state->bb_dec_1);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_2);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_3);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_4);
	fir_cic3_decim_2_s16_s16_init(&state->bb_dec_5);
	// TODO: Channel filter here.
	fir_64_decim_2_real_s16_s16_init(&state->audio_dec, taps_64_lp_031_063, 64);
}

void rx_am_to_audio_baseband_handler(void* const _state, complex_s8_t* const in, const size_t sample_count_in, void* const out, baseband_timestamps_t* const timestamps) {
	rx_am_to_audio_state_t* const state = (rx_am_to_audio_state_t*)_state;

	size_t sample_count = sample_count_in;

	timestamps->start = baseband_timestamp();

	/* 3.072MHz complex<int8>[N]
	 * -> Shift by -fs/4
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 1.544MHz complex<int16>[N/2] */
	sample_count = translate_fs_over_4_and_decimate_by_2_cic_3_s8_s16(&state->bb_dec_1, in, sample_count);

	/* 1.544MHz complex<int16>[N/2]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 768kHz complex<int16>[N/4] */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_2, (complex_s16_t*)in, out, sample_count);

	/* TODO: Gain through five CICs will be 32768 (8 ^ 5). Incorporate gain adjustment
	 * somewhere in the chain.
	 */

	/* TODO: Create a decimate_by_2_s16_s16 with gain adjustment and rounding, maybe use
	 * SSAT (no rounding) or use SMMULR/SMMLAR/SMMLSR (provides rounding)?
	 */

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
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
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_3, out, out, sample_count);

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
	p = (complex_s16_t*)out;
	for(uint_fast16_t n=sample_count; n>0; n-=1) {
		p->i /= 8;
		p->q /= 8;
		p++;
	}

	/* 384kHz complex<int16>[N/8]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 192kHz complex<int16>[N/16] */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_4, out, out, sample_count);

	/* Temporary code to adjust gain in complex_s16_t samples out of CIC stages */
	p = (complex_s16_t*)out;
	for(uint_fast16_t n=sample_count; n>0; n-=1) {
		p->i /= 8;
		p->q /= 8;
		p++;
	}

	/* 192kHz complex<int16>[N/16]
	 * -> 3rd order CIC decimation by 2, gain of 8
	 * -> 96kHz complex<int16>[N/32] */
	sample_count = fir_cic3_decim_2_s16_s16(&state->bb_dec_5, out, out, sample_count);

	timestamps->decimate_end = baseband_timestamp();

	// TODO: Design a proper channel filter.

	timestamps->channel_filter_end = baseband_timestamp();

	/* 96kHz int16[N/32]
	 * -> AM demodulation
	 * -> 96kHz int16[N/32] */
	am_demodulate_s16_s16(out, out, sample_count);

	timestamps->demodulate_end = baseband_timestamp();

	/* 96kHz int16[N/32]
	 * -> FIR filter, gain of 1
	 * -> 48kHz int16[N/64] */
	sample_count = fir_64_decim_2_real_s16_s16(&state->audio_dec, out, out, sample_count);

	timestamps->audio_end = baseband_timestamp();

	copy_to_audio_output(out, sample_count);
}

typedef struct specan_state_t {
} specan_state_t;

void specan_init(void* const _state) {
	specan_state_t* const state = (specan_state_t*)_state;
	(void)state;
}

void specan_baseband_handler(void* const _state, complex_s8_t* const in, const size_t sample_count_in, void* const out, baseband_timestamps_t* const timestamps) {
	specan_state_t* const state = (specan_state_t*)_state;
	(void)state;
	(void)in;
	(void)sample_count_in;
	(void)out;
	(void)timestamps;
}

static volatile receiver_baseband_handler_t receiver_baseband_handler = NULL;

typedef enum {
	RECEIVER_CONFIGURATION_SPEC = 0,
	RECEIVER_CONFIGURATION_NBAM = 1,
	RECEIVER_CONFIGURATION_NBFM = 2,
	RECEIVER_CONFIGURATION_WBFM = 3,
	RECEIVER_CONFIGURATION_TPMS = 4,
	RECEIVER_CONFIGURATION_TPMS_FSK = 5,
} receiver_configuration_id_t;

const receiver_configuration_t receiver_configurations[] = {
	[RECEIVER_CONFIGURATION_SPEC] = {
		.name = "SPEC",
		.init = specan_init,
		.baseband_handler = specan_baseband_handler,
		.tuning_offset = 0,
		.sample_rate = 15000000,
		.baseband_bandwidth = 2500000,
		.baseband_decimation = 3,
		.enable_audio = false,
		.enable_spectrum = true,
	},
	[RECEIVER_CONFIGURATION_NBAM] = {
		.name = "NBAM",
		.init = rx_am_to_audio_init,
		.baseband_handler = rx_am_to_audio_baseband_handler,
		.tuning_offset = -768000,
		.sample_rate = 12288000,
		.baseband_bandwidth = 1750000,
		.baseband_decimation = 4,
		.enable_audio = true,
		.enable_spectrum = false,
	},
	[RECEIVER_CONFIGURATION_NBFM] = {
		.name = "NBFM",
		.init = rx_fm_narrowband_to_audio_init,
		.baseband_handler = rx_fm_narrowband_to_audio_baseband_handler,
		.tuning_offset = -768000,
		.sample_rate = 12288000,
		.baseband_bandwidth = 1750000,
		.baseband_decimation = 4,
		.enable_audio = true,
		.enable_spectrum = false,
	},
	[RECEIVER_CONFIGURATION_WBFM] = {
		.name = "WBFM",
		.init = rx_fm_broadcast_to_audio_init,
		.baseband_handler = rx_fm_broadcast_to_audio_baseband_handler,
		.tuning_offset = -768000,
		.sample_rate = 12288000,
		.baseband_bandwidth = 1750000,
		.baseband_decimation = 4,
		.enable_audio = true,
		.enable_spectrum = false,
	},
	[RECEIVER_CONFIGURATION_TPMS] = {
		.name = "TPMS-ASK",
		.init = rx_tpms_ask_init_wrapper,
		.baseband_handler = rx_tpms_ask_baseband_handler,
		.tuning_offset = -768000,
		.sample_rate = 12288000,
		.baseband_bandwidth = 1750000,
		.baseband_decimation = 4,
		.enable_audio = true,
		.enable_spectrum = false,
	},
	[RECEIVER_CONFIGURATION_TPMS_FSK] = {
		.name = "TPMS-FSK",
		.init = rx_tpms_fsk_init_wrapper,
		.baseband_handler = rx_tpms_fsk_baseband_handler,
		.tuning_offset = -614400,
		.sample_rate = 9830400,
		.baseband_bandwidth = 1750000,
		.baseband_decimation = 4,
		.enable_audio = true,
		.enable_spectrum = false,
	},
};

const receiver_configuration_t* get_receiver_configuration() {
	return &receiver_configurations[device_state->receiver_configuration_index];
}

static complex_s8_t* get_completed_baseband_buffer() {
	const size_t current_lli_index = sgpio_dma_current_transfer_index(lli_rx, 2);
	const size_t finished_lli_index = 1 - current_lli_index;
	return lli_rx[finished_lli_index].cdestaddr;
}

complex_s8_t* wait_for_completed_baseband_buffer() {
	const size_t last_lli_index = sgpio_dma_current_transfer_index(lli_rx, 2);
	while( sgpio_dma_current_transfer_index(lli_rx, 2) == last_lli_index );
	return get_completed_baseband_buffer();
}

bool set_frequency(const int64_t new_frequency) {
	const receiver_configuration_t* const receiver_configuration = get_receiver_configuration();

	const int64_t tuned_frequency = new_frequency + receiver_configuration->tuning_offset;
	if( set_freq(tuned_frequency) ) {
		device_state->tuned_hz = new_frequency;
		return true;
	} else {
		return false;
	}
}

void increment_frequency(const int32_t increment) {
	const int64_t new_frequency = device_state->tuned_hz + increment;
	set_frequency(new_frequency);
}

static uint8_t receiver_state_buffer[1024];

void set_rx_mode(const uint32_t new_receiver_configuration_index) {
	if( new_receiver_configuration_index >= ARRAY_SIZE(receiver_configurations) ) {
		return;
	}
	
	// TODO: Mute audio, clear audio buffers?
	i2s_mute();

	sgpio_dma_stop();
	sgpio_cpld_stream_disable();

	/* TODO: Ensure receiver_state_buffer is large enough for new mode, or start using
	 * heap to allocate necessary memory.
	 */
	const receiver_configuration_t* const old_receiver_configuration = get_receiver_configuration();
	device_state->receiver_configuration_index = new_receiver_configuration_index;
	const receiver_configuration_t* const receiver_configuration = get_receiver_configuration();

	if( old_receiver_configuration->tuning_offset != receiver_configuration->tuning_offset ) {
		set_frequency(device_state->tuned_hz);
	}

	sample_rate_set(receiver_configuration->sample_rate);
	baseband_filter_bandwidth_set(receiver_configuration->baseband_bandwidth);
	sgpio_cpld_stream_rx_set_decimation(receiver_configuration->baseband_decimation);

	receiver_configuration->init(receiver_state_buffer);
	receiver_baseband_handler = receiver_configuration->baseband_handler;

	sgpio_dma_rx_start(&lli_rx[0]);
	sgpio_cpld_stream_enable();

	if( receiver_configuration->enable_audio ) {
		i2s_unmute();
	}
}

#include "portapack_driver.h"

void portapack_init() {
	cpu_clock_pll1_max_speed();
	
	portapack_cpld_jtag_io_init();
	portapack_cpld_jtag_reset();

	device_state->tuned_hz = 162550000;
	device_state->lna_gain_db = 0;
	device_state->if_gain_db = 32;
	device_state->bb_gain_db = 32;
	device_state->audio_out_gain_db = 0;
	device_state->receiver_configuration_index = RECEIVER_CONFIGURATION_SPEC;

	portapack_encoder_init();

	ipc_channel_init(&device_state->ipc_m4, ipc_m4_buffer, ipc_m4_buffer_size);
	ipc_channel_init(&device_state->ipc_m0, ipc_m0_buffer, ipc_m0_buffer_size);

	portapack_i2s_init();

	sgpio_set_slice_mode(false);

	rf_path_init();
	rf_path_set_direction(RF_PATH_DIRECTION_RX);

	rf_path_set_lna((device_state->lna_gain_db >= 14) ? 1 : 0);
	max2837_set_lna_gain(device_state->if_gain_db);	/* 8dB increments */
	max2837_set_vga_gain(device_state->bb_gain_db);	/* 2dB increments, up to 62dB */

	m0_load_code_from_m4_text();
	m0_run();

	systick_set_reload(0xffffff); 
	systick_set_clocksource(1);
	systick_counter_enable();

	sgpio_dma_init();

	sgpio_dma_configure_lli(&lli_rx[0], 1, false, sample_buffer_0, 4096);
	sgpio_dma_configure_lli(&lli_rx[1], 1, false, sample_buffer_1, 4096);

	gpdma_lli_create_loop(&lli_rx[0], 2);

	gpdma_lli_enable_interrupt(&lli_rx[0]);
	gpdma_lli_enable_interrupt(&lli_rx[1]);

	nvic_set_priority(NVIC_DMA_IRQ, 0);
	nvic_enable_irq(NVIC_DMA_IRQ);

	nvic_set_priority(NVIC_M0CORE_IRQ, 255);
	nvic_enable_irq(NVIC_M0CORE_IRQ);

	set_rx_mode(RECEIVER_CONFIGURATION_SPEC);

	set_frequency(device_state->tuned_hz);
}

static volatile uint32_t sample_frame_count = 0;

void dma_isr() {
	sgpio_dma_irq_tc_acknowledge();
	sample_frame_count += 1;

	complex_s8_t* const completed_buffer = get_completed_baseband_buffer();

	/* 12.288MHz
	 * -> CPLD decimation by 4
	 * -> 3.072MHz complex<int8>[2048] == 666.667 usec/block == 136000 cycles/sec
	 */
	if( receiver_baseband_handler ) {
		int16_t work[2048];
		baseband_timestamps_t timestamps;
		receiver_baseband_handler(receiver_state_buffer, completed_buffer, 2048, work, &timestamps);

		device_state->dsp_metrics.duration_decimate = systick_difference(timestamps.start, timestamps.decimate_end);
		device_state->dsp_metrics.duration_channel_filter = systick_difference(timestamps.decimate_end, timestamps.channel_filter_end);
		device_state->dsp_metrics.duration_demodulate = systick_difference(timestamps.channel_filter_end, timestamps.demodulate_end);
		device_state->dsp_metrics.duration_audio = systick_difference(timestamps.demodulate_end, timestamps.audio_end);
		device_state->dsp_metrics.duration_all = systick_difference(timestamps.start, timestamps.audio_end);

		static const float cycles_per_baseband_block = (2048.0f / 3072000.0f) * 200000000.0f;
		device_state->dsp_metrics.duration_all_millipercent = (float)device_state->dsp_metrics.duration_all / cycles_per_baseband_block * 100000.0f;
	}
}

#include "arm_intrinsics.h"

void portapack_run() {
	__WFE();
}
