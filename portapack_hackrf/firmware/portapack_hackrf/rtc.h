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

#ifndef __RTC_H__
#define __RTC_H__

#include <stdint.h>
 
void rtc_init();

void rtc_enable();
void rtc_disable();

void rtc_date_set(const uint_fast16_t year, const uint_fast8_t month, const  uint_fast8_t day_of_month);
void rtc_time_set(const uint_fast8_t hour, const uint_fast8_t minute, const uint_fast8_t second);

uint_fast16_t rtc_year();
uint_fast8_t rtc_month();
uint_fast16_t rtc_day_of_year();
uint_fast8_t rtc_day_of_week();
uint_fast8_t rtc_day_of_month();
uint_fast8_t rtc_hour();
uint_fast8_t rtc_minute();
uint_fast8_t rtc_second();

void rtc_counter_interrupt_second_enable();
void rtc_counter_interrupt_enable();
void rtc_counter_interrupt_clear();

#endif/*__RTC_H__*/
