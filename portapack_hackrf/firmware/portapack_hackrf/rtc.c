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

#include <libopencm3/lpc43xx/rtc.h>
#include <libopencm3/lpc43xx/creg.h>

#include <delay.h>

#include <stdint.h>

void rtc_enable() {
	RTC_CCR |= RTC_CCR_CLKEN(1);
}

void rtc_disable() {
	RTC_CCR &= ~RTC_CCR_CLKEN(1);
}

static bool clock_32k768_is_running() {
	const uint32_t creg0_masked = CREG_CREG0 & (CREG_CREG0_PD32KHZ | CREG_CREG0_RESET32KHZ);
	const uint32_t creg0_expected = 0;
	return (creg0_masked == creg0_expected);
}

static void clock_32k768_init() {
	// Enable 32.768kHz oscillator
	CREG_CREG0 &= ~CREG_CREG0_PD32KHZ;

	// Clear 32.768kHz oscillator reset
	CREG_CREG0 &= ~CREG_CREG0_RESET32KHZ;
}

void rtc_init() {
	if( !clock_32k768_is_running() ) {
		clock_32k768_init();

		/* TODO: Call a proper 2 second delay function. */
		volatile uint32_t ms = 2000;
		while((ms--) > 0) {
			delay(50000);
		}
	}

	// Disable 32.768kHz output from 32.768kHz oscillator.
	CREG_CREG0 &= ~CREG_CREG0_EN32KHZ;

	// Enable 1kHz output from 32.768kHz oscillator
	CREG_CREG0 |= CREG_CREG0_EN1KHZ;

	rtc_enable();
}

void rtc_date_set(const uint_fast16_t year, const uint_fast8_t month, const  uint_fast8_t day_of_month) {
	rtc_disable();
	RTC_YEAR = year;
	RTC_MONTH = month;
	RTC_DOM = day_of_month;
	rtc_enable();
}

void rtc_time_set(const uint_fast8_t hour, const uint_fast8_t minute, const uint_fast8_t second) {
	rtc_disable();
	RTC_HRS = hour;
	RTC_MIN = minute;
	RTC_SEC = second;
	rtc_enable();
}

uint_fast16_t rtc_year() {
	return RTC_YEAR;
}

uint_fast8_t rtc_month() {
	return RTC_MONTH;
}

uint_fast16_t rtc_day_of_year() {
	return RTC_DOY;
}

uint_fast8_t rtc_day_of_week() {
	return RTC_DOW;
}

uint_fast8_t rtc_day_of_month() {
	return RTC_DOM;
}

uint_fast8_t rtc_hour() {
	return RTC_HRS;
}

uint_fast8_t rtc_minute() {
	return RTC_MIN;
}

uint_fast8_t rtc_second() {
	return RTC_SEC;
}
