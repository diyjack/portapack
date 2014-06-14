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

#ifndef __WM8731_H__
#define __WM8731_H__

#define WM8731_I2C_ADDR 0b00110100

#define WM8731_LIN (0b0000000)
#define WM8731_LIN_LINVOL(x) ((x) << 0)
#define WM8731_LIN_LINMUTE(x) ((x) << 7)
#define WM8731_LIN_LRINBOTH(x) ((x) << 8)

#define WM8731_RIN (0b0000001)
#define WM8731_RIN_RINVOL(x) ((x) << 0)
#define WM8731_RIN_RINMUTE(x) ((x) << 7)
#define WM8731_RIN_RLINBOTH(x) ((x) << 8)

#define WM8731_LHP (0b0000010)
#define WM8731_LHP_LHPVOL(x) ((x) << 0)
#define WM8731_LHP_LZCEN(x) ((x) << 7)
#define WM8731_LHP_LRHPBOTH(x) ((x) << 8)

#define WM8731_RHP (0b0000011)
#define WM8731_RHP_RHPVOL(x) ((x) << 0)
#define WM8731_RHP_RZCEN(x) ((x) << 7)
#define WM8731_RHP_RLHPBOTH(x) ((x) << 8)

#define WM8731_APATH (0b0000100)
#define WM8731_APATH_MICBOOST(x) ((x) << 0)
#define WM8731_APATH_MUTEMIC(x) ((x) << 1)
#define WM8731_APATH_INSEL(x) ((x) << 2)
#define WM8731_APATH_BYPASS(x) ((x) << 3)
#define WM8731_APATH_DACSEL(x) ((x) << 4)
#define WM8731_APATH_SIDETONE(x) ((x) << 5)
#define WM8731_APATH_SIDEATT(x) ((x) << 6)

#define WM8731_DPATH (0b0000101)
#define WM8731_DPATH_ADCHPD(x) ((x) << 0)
#define WM8731_DPATH_DEEMP(x) ((x) << 1)
#define WM8731_DPATH_DACMU(x) ((x) << 3)
#define WM8731_DPATH_HPOR(x) ((x) << 4)

#define WM8731_PD (0b0000110)
#define WM8731_PD_LINEINPD(x) ((x) << 0)
#define WM8731_PD_MICPD(x) ((x) << 1)
#define WM8731_PD_ADCPD(x) ((x) << 2)
#define WM8731_PD_DACPD(x) ((x) << 3)
#define WM8731_PD_OUTPD(x) ((x) << 4)
#define WM8731_PD_OSCPD(x) ((x) << 5)
#define WM8731_PD_CLKOUTPD(x) ((x) << 6)
#define WM8731_PD_POWEROFF(x) ((x) << 7)

#define WM8731_DAI (0b0000111)
#define WM8731_DAI_FORMAT(x) ((x) << 0)
#define WM8731_DAI_IWL(x) ((x) << 2)
#define WM8731_DAI_LRP(x) ((x) << 4)
#define WM8731_DAI_LRSWAP(x) ((x) << 5)
#define WM8731_DAI_MS(x) ((x) << 6)
#define WM8731_DAI_BCLKINV(x) ((x) << 7)

#define WM8731_SAMP (0b0001000)
#define WM8731_SAMP_USB(x) ((x) << 0)
#define WM8731_SAMP_BOSR(x) ((x) << 1)
#define WM8731_SAMP_SR(x) ((x) << 2)
#define WM8731_SAMP_CLKIDIV2(x) ((x) << 6)
#define WM8731_SAMP_CLKODIV2(x) ((x) << 7)

#define WM8731_ACTIVE (0b0001001)
#define WM8731_ACTIVE_ACTIVE(x) ((x) << 0)

#define WM8731_RESET (0b0001111)
#define WM8731_RESET_RESET(x) ((x) << 0)

#endif//__WM8731_H__
