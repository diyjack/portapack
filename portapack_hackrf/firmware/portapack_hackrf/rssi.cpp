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

#include "rssi.h"

#include <libopencm3/lpc43xx/adc.h>

#include <algorithm>

/* TODO: Move to libopencm3! */
#define ADC_CR_SEL(x)		((x) << 0)
#define ADC_CR_CLKDIV(x)	((x) << 8)
#define ADC_CR_BURST(x)		((x) << 16)

#define ADC_CR_CLKS(x)		((x) << 17)
#define ADC_CR_CLKS_11_10_BITS (0)

#define ADC_CR_PDN(x)		((x) << 21)
#define ADC_CR_PDN_OPERATIONAL	(1)

#define ADC_CR_START(x)		((x) << 24)
#define ADC_CR_START_NO_START	(0)
#define ADC_CR_START_NOW	(1)

#define ADC_CR_EDGE(x)		((x) << 27)
#define ADC_CR_EDGE_RISING		(0)

#define ADC_INTEN_ADINTEN(x)	((x) << 0)
#define ADC_INTEN_ADGINTEN(x)	((x) << 8)

#define RSSI_ADC_INPUT (1)
#define RSSI_ADC_RESOLUTION (1024)
#define RSSI_ADC_FULLSCALE_V (3.3f)
#define RSSI_ADC_V_PER_LSB (RSSI_ADC_FULLSCALE_V / RSSI_ADC_RESOLUTION)
#define RSSI_V_MIN (0.4f)
#define RSSI_V_MAX (2.2f)
#define RSSI_V_PER_DB (0.03f)
#define RSSI_MINIMUM_VALUE ((int)(RSSI_ADC_RESOLUTION * RSSI_V_MIN / RSSI_ADC_FULLSCALE_V + 0.5f))
#define RSSI_MAXIMUM_VALUE ((int)(RSSI_ADC_RESOLUTION * RSSI_V_MAX / RSSI_ADC_FULLSCALE_V + 0.5f))
#define RSSI_ADC_RANGE (RSSI_MAXIMUM_VALUE - RSSI_MINIMUM_VALUE)
#define RSSI_MILLIDB_PER_LSB ((int)(1000 * RSSI_ADC_V_PER_LSB / RSSI_V_PER_DB + 0.5f))

void rssi_init() {
	ADC1_CR =
		  ADC_CR_SEL(
		  	  (1 << RSSI_ADC_INPUT)
		  )
		| ADC_CR_CLKDIV(47)	/* 204MHz / 48 = 4.25 MHz */
		| ADC_CR_BURST(0)
		| ADC_CR_CLKS(ADC_CR_CLKS_11_10_BITS)
		| ADC_CR_PDN(ADC_CR_PDN_OPERATIONAL)
		| ADC_CR_START(ADC_CR_START_NO_START)
		| ADC_CR_EDGE(ADC_CR_EDGE_RISING)
		;
}

void rssi_convert_start() {
	ADC1_CR =
		  ADC_CR_SEL(
		 	(1 << RSSI_ADC_INPUT)
		  )
		| ADC_CR_CLKDIV(47) /* 204MHz / 48 = 4.25 MHz */
		| ADC_CR_BURST(0)
		| ADC_CR_CLKS(ADC_CR_CLKS_11_10_BITS)
		| ADC_CR_PDN(ADC_CR_PDN_OPERATIONAL)
		| ADC_CR_START(ADC_CR_START_NOW)
		| ADC_CR_EDGE(ADC_CR_EDGE_RISING)
		;

}

uint32_t rssi_read() {
	//while((ADC1_GDR & (1UL << 31)) == 0);

	return (ADC1_GDR >> 6) & 0x3ff;
}

int32_t rssi_raw_to_millidb(const uint32_t raw) {
	return std::min((uint32_t)(RSSI_ADC_RANGE * RSSI_MILLIDB_PER_LSB), std::max((uint32_t)0, raw - RSSI_MINIMUM_VALUE) * RSSI_MILLIDB_PER_LSB);
}
