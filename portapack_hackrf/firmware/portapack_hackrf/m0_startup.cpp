/*
 * Copyright (C) 2013 Jared Boone <jared@sharebrained.com>
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

#include "m0_startup.h"

#include <libopencm3/lpc43xx/cgu.h>
#include <libopencm3/lpc43xx/creg.h>
#include <libopencm3/lpc43xx/rgu.h>
#include <libopencm3/lpc43xx/scu.h>
#include <libopencm3/lpc43xx/spifi.h>
#include <hackrf_core.h>

extern unsigned __m0_start__, __m0_end__;

void m0_load_code_from_m4_text() {
	// Reset M0 and hold in reset.
	RESET_CTRL1 = RESET_CTRL1_M0APP_RST;

	// Change Shadow memory to Real RAM
	CREG_M0APPMEMMAP = 0x20000000;

	// TODO: Add a barrier here due to change in memory map?
	//__builtin_dsb();

	volatile unsigned *src, *dest;
	dest = (unsigned*)CREG_M0APPMEMMAP;
	if( dest != &__m0_start__ ) {
		for(src = &__m0_start__; src < &__m0_end__; )
		{
			*dest++ = *src++;
		}
	}
}

void m0_configure_for_spifi() {
	// Reset M0 and hold in reset.
	RESET_CTRL1 = RESET_CTRL1_M0APP_RST;

	SPIFI_CTRL =
		  SPIFI_CTRL_DMAEN(0)
		| SPIFI_CTRL_FBCLK(1)
		| SPIFI_CTRL_RFCLK(1)
		| SPIFI_CTRL_DUAL(0)
		| SPIFI_CTRL_PRFTCH_DIS(0)
		| SPIFI_CTRL_MODE3(0)
		| SPIFI_CTRL_INTEN(0)
		| SPIFI_CTRL_D_PRFTCH_DIS(0)
		| SPIFI_CTRL_CSHIGH(1)			// Two serial clocks, ~20ns
		| SPIFI_CTRL_TIMEOUT(0xffff)
		;
/*
	SPIFI_CLIMIT = 0x10000000;

	SPIFI_MCMD =
		  SPIFI_MCMD_OPCODE(0xeb)
		| SPIFI_MCMD_FRAMEFORM(0)
		| SPIFI_MCMD_FIELDFORM(2)	// Serial opcode. Other fields are quad.
		| SPIFI_MCMD_INTLEN(3)		// Intermediate bytes before data.
		| SPIFI_MCMD_DOUT(0)
		| SPIFI_MCMD_POLL(0)
		;
*/
	/* Ensure pins are configured for maximum I/O rate */
	scu_pinmux(SCU_SSP0_MISO,  SCU_SSP_IO | SCU_CONF_FUNCTION3);
	scu_pinmux(SCU_SSP0_MOSI,  SCU_SSP_IO | SCU_CONF_FUNCTION3);
	scu_pinmux(SCU_SSP0_SCK,   SCU_SSP_IO | SCU_CONF_FUNCTION3);
	scu_pinmux(SCU_SSP0_SSEL,  SCU_SSP_IO | SCU_CONF_FUNCTION3);
	scu_pinmux(SCU_FLASH_HOLD, SCU_SSP_IO | SCU_CONF_FUNCTION3);
	scu_pinmux(SCU_FLASH_WP,   SCU_SSP_IO | SCU_CONF_FUNCTION3);

	/* Throttle up the SPIFI interface */
	CGU_IDIVB_CTRL =
		  CGU_IDIVB_CTRL_CLK_SEL(9)		/* PLL1 */
		| CGU_IDIVB_CTRL_AUTOBLOCK(1)
		| CGU_IDIVB_CTRL_IDIV(1)		/* Divide by 2, to 100MHz */
		| CGU_IDIVB_CTRL_PD(0)
		;

	CREG_M0APPMEMMAP = 0x14000000 + 0x20000;
}

void m0_run() {
	// Release M0 from reset.
	RESET_CTRL1 = 0;
}
