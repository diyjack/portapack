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

#include "audio.h"
#include "i2s.h"

#include "portapack_driver.h"

#include "wm8731.h"

void portapack_codec_init() {
	/* Reset */
	portapack_audio_codec_write(WM8731_RESET,
		WM8731_RESET_RESET(0)
	);

	/* Power */
	portapack_audio_codec_write(WM8731_PD,
		  WM8731_PD_LINEINPD(1)
		| WM8731_PD_MICPD(0)
		| WM8731_PD_ADCPD(0)
		| WM8731_PD_DACPD(0)
		| WM8731_PD_OUTPD(0)
		| WM8731_PD_OSCPD(1)
		| WM8731_PD_CLKOUTPD(1)
		| WM8731_PD_POWEROFF(0)
	);

	/* Sampling control */
	portapack_audio_codec_write(WM8731_SAMP,
		  WM8731_SAMP_USB(0)
		| WM8731_SAMP_BOSR(0)
		| WM8731_SAMP_SR(0)
		| WM8731_SAMP_CLKIDIV2(0)
		| WM8731_SAMP_CLKODIV2(0)
	);

	/* Digital audio interface format */
	portapack_audio_codec_write(WM8731_DAI,
		  WM8731_DAI_FORMAT(2)
		| WM8731_DAI_IWL(0)
		| WM8731_DAI_LRP(0)
		| WM8731_DAI_LRSWAP(0)
		| WM8731_DAI_MS(0)
		| WM8731_DAI_BCLKINV(0)
	);

	/* Digital audio path control */
	portapack_audio_codec_write(WM8731_DPATH,
		  WM8731_DPATH_ADCHPD(0)
		| WM8731_DPATH_DEEMP(0)
		| WM8731_DPATH_DACMU(0)
		| WM8731_DPATH_HPOR(0)
	);

	/* Analog audio path control */
	portapack_audio_codec_write(WM8731_APATH,
		  WM8731_APATH_MICBOOST(0)
		| WM8731_APATH_MUTEMIC(0)
		| WM8731_APATH_INSEL(1)
		| WM8731_APATH_BYPASS(0)
		| WM8731_APATH_DACSEL(1)
		| WM8731_APATH_SIDETONE(0)
		| WM8731_APATH_SIDEATT(0)
	);

	/* Active control */
	portapack_audio_codec_write(WM8731_ACTIVE,
		WM8731_ACTIVE_ACTIVE(1)
	);

	portapack_audio_codec_write(WM8731_LIN,
		  WM8731_LIN_LINVOL(0b10111)
		| WM8731_LIN_LINMUTE(1)
		| WM8731_LIN_LRINBOTH(0)
	);
	portapack_audio_codec_write(WM8731_RIN,
		  WM8731_RIN_RINVOL(0b10111)
		| WM8731_RIN_RINMUTE(1)
		| WM8731_RIN_RLINBOTH(0)
	);

	portapack_audio_codec_write(WM8731_LHP,
		  WM8731_LHP_LHPVOL(0b1111001)
		| WM8731_LHP_LZCEN(0)
		| WM8731_LHP_LRHPBOTH(0)
	);
	portapack_audio_codec_write(WM8731_RHP,
		  WM8731_RHP_RHPVOL(0b1111001)
		| WM8731_RHP_RZCEN(0)
		| WM8731_RHP_RLHPBOTH(0)
	);
}

int_fast8_t portapack_audio_out_volume_set(int_fast8_t db) {
	if( db > 6 ) {
		db = 6;
	}
	if( db < -74 ) {
		db = -74;
	}
	const uint_fast16_t v = db + 121;
	portapack_audio_codec_write(WM8731_LHP,
		  WM8731_LHP_LHPVOL(v)
		| WM8731_LHP_LZCEN(1)
		| WM8731_LHP_LRHPBOTH(1)
	);
	return db;
}

void portapack_audio_out_mute() {
	portapack_audio_codec_write(WM8731_LHP,
		  WM8731_LHP_LHPVOL(0)
		| WM8731_LHP_LZCEN(0)
		| WM8731_LHP_LRHPBOTH(1)
	);
}
