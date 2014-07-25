# Copyright 2014 Jared Boone <jared@sharebrained.com>
#
# This file is part of PortaPack.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

# Usage:
#	cmake -DRUN_FROM=RAM -DPATH_HACKRF=../../../../hackrf ..

cmake_minimum_required(VERSION 2.8.9)
set(CMAKE_TOOLCHAIN_FILE ../../../../hackrf/firmware/toolchain-arm-cortex-m.cmake)

project(portapack_hackrf)

include(../../../../hackrf/firmware/hackrf-common.cmake)

set(SRC_M4
	main.c
	portapack.c
	portapack_driver.c
	access_code_correlator.c
	integrator.c
	envelope.c
	clock_recovery.c
	packet_builder.c
	audio.c
	cpld.c
	i2s.c
	complex.c
	fft.c
	fxpt_atan2.c
	decimate.c
	demodulate.c
	kfifo.c
	ipc.c
	ipc_m0_client.c
	ipc_m4_server.c
	m0_startup.c
	${PATH_HACKRF_FIRMWARE_COMMON}/rf_path.c
	${PATH_HACKRF_FIRMWARE_COMMON}/tuning.c
	${PATH_HACKRF_FIRMWARE_COMMON}/streaming.c
	${PATH_HACKRF_FIRMWARE_COMMON}/fault_handler.c
	${PATH_HACKRF_FIRMWARE_COMMON}/hackrf_core.c
	${PATH_HACKRF_FIRMWARE_COMMON}/sgpio.c
	${PATH_HACKRF_FIRMWARE_COMMON}/gpdma.c
	${PATH_HACKRF_FIRMWARE_COMMON}/sgpio_dma.c
	${PATH_HACKRF_FIRMWARE_COMMON}/si5351c.c
	${PATH_HACKRF_FIRMWARE_COMMON}/max2837.c
	${PATH_HACKRF_FIRMWARE_COMMON}/max5864.c
	${PATH_HACKRF_FIRMWARE_COMMON}/rffc5071.c
	${PATH_HACKRF_FIRMWARE_COMMON}/delay.c
	${PATH_HACKRF_FIRMWARE_COMMON}/led.c
	#${PATH_HACKRF_FIRMWARE_COMMON}/usb.c
	#${PATH_HACKRF_FIRMWARE_COMMON}/usb_queue.c
	#${PATH_HACKRF_FIRMWARE_COMMON}/usb_request.c
	#${PATH_HACKRF_FIRMWARE_COMMON}/usb_standard_request.c
	#${PATH_HACKRF_FIRMWARE_HACKRF_USB}/usb_device.c
	#${PATH_HACKRF_FIRMWARE_HACKRF_USB}/usb_descriptor.c
	#${PATH_HACKRF_FIRMWARE_HACKRF_USB}/usb_endpoint.c
)

set(SRC_M0
	portapack_driver.c
	lcd.c
	lcd_color_lut.c
	lcd_loop.c
	lcd_touch.c
	font_fixed_8x16.c
	console.c
	kfifo.c
	ipc.c
	ipc_m4_client.c
	ipc_m0_server.c
	rtc.c
	sdio.c
	manchester.c
	${PATH_HACKRF_FIRMWARE_COMMON}/delay.c
	${PATH_HACKRF_FIRMWARE_COMMON}/led.c
)

DeclareTargets()
