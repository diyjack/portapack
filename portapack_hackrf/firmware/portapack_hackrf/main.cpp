/*
 * Copyright (C) 2012 Jared Boone, ShareBrained Technology, Inc.
 * Copyright 2013 Benjamin Vernoux
 *
 * This file is part of PortaPack. It was derived from HackRF.
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

#include <stddef.h>

#include <libopencm3/lpc43xx/m4/nvic.h>
#include <libopencm3/lpc43xx/rgu.h>

#include <hackrf_core.h>

#include "portapack.h"

#include "portapack_driver.h"

/*
#include "usb.h"
#include "usb_standard_request.h"

#include "usb_device.h"
#include "usb_endpoint.h"

usb_request_status_t usb_vendor_request_jtag_sequence(
	usb_endpoint_t* const endpoint,
	const usb_transfer_stage_t stage) {
	if( stage == USB_TRANSFER_STAGE_SETUP ) {
		//if( endpoint->setup.index < MAX2837_NUM_REGS ) {
		//	if( endpoint->setup.value < MAX2837_DATA_REGS_MAX_VALUE ) {
		//		max2837_reg_write(endpoint->setup.index, endpoint->setup.value);
				usb_transfer_schedule_ack(endpoint->in);
				return USB_REQUEST_STATUS_OK;
		//	}
		//}
		//return USB_REQUEST_STATUS_STALL;
	} else {
		return USB_REQUEST_STATUS_OK;
	}
}

static const usb_request_handler_fn vendor_request_handler[] = {
	usb_vendor_request_jtag_sequence,
};

static const uint32_t vendor_request_handler_count =
	sizeof(vendor_request_handler) / sizeof(vendor_request_handler[0]);

usb_request_status_t usb_vendor_request(
	usb_endpoint_t* const endpoint,
	const usb_transfer_stage_t stage
) {
	usb_request_status_t status = USB_REQUEST_STATUS_STALL;
	
	if( endpoint->setup.request < vendor_request_handler_count ) {
		usb_request_handler_fn handler = vendor_request_handler[endpoint->setup.request];
		if( handler ) {
			status = handler(endpoint, stage);
		}
	}
	
	return status;
}

const usb_request_handlers_t usb_request_handlers = {
	.standard = usb_standard_request,
	.class = 0,
	.vendor = usb_vendor_request,
	.reserved = 0,
};

void usb_configuration_changed(
	usb_device_t* const device
) {
	led_set(LED1, device->configuration->number == 1);
}
*/
#ifdef LCD_BACKLIGHT_TEST
#include <led.h>
#endif

#ifdef CPLD_PROGRAM
#include <led.h>
#include "cpld.h"
#endif

int main(void) {
	RESET_CTRL0 =
		  RESET_CTRL0_SCU_RST
		| RESET_CTRL0_LCD_RST
		| RESET_CTRL0_USB0_RST
		| RESET_CTRL0_USB1_RST
		| RESET_CTRL0_DMA_RST
		| RESET_CTRL0_SDIO_RST
		| RESET_CTRL0_EMC_RST
		| RESET_CTRL0_ETHERNET_RST
		| RESET_CTRL0_GPIO_RST
		;
	
	RESET_CTRL1 =
		  RESET_CTRL1_TIMER0_RST
		| RESET_CTRL1_TIMER1_RST
		| RESET_CTRL1_TIMER2_RST
		| RESET_CTRL1_TIMER3_RST
		| RESET_CTRL1_RTIMER_RST
		| RESET_CTRL1_SCT_RST
		| RESET_CTRL1_MOTOCONPWM_RST
		| RESET_CTRL1_QEI_RST
		| RESET_CTRL1_ADC0_RST
		| RESET_CTRL1_ADC1_RST
		| RESET_CTRL1_DAC_RST
		| RESET_CTRL1_UART0_RST
		| RESET_CTRL1_UART1_RST
		| RESET_CTRL1_UART2_RST
		| RESET_CTRL1_UART3_RST
		| RESET_CTRL1_I2C0_RST
		| RESET_CTRL1_I2C1_RST
		| RESET_CTRL1_SSP0_RST
		| RESET_CTRL1_SSP1_RST
		| RESET_CTRL1_I2S_RST
		| RESET_CTRL1_SPIFI_RST
		| RESET_CTRL1_CAN1_RST
		| RESET_CTRL1_CAN0_RST
		| RESET_CTRL1_M0APP_RST
		| RESET_CTRL1_SGPIO_RST
		| RESET_CTRL1_SPI_RST
		;
	
	NVIC_ICER(0) = 0xffffffff;
	NVIC_ICER(1) = 0xffffffff;

	NVIC_ICPR(0) = 0xffffffff;
	NVIC_ICPR(1) = 0xffffffff;

	pin_setup();
	enable_1v8_power();
#ifdef HACKRF_ONE
	enable_rf_power();
#endif
	cpu_clock_init();
/*
cpu_clock_pll1_max_speed();
portapack_cpld_jtag_io_init();
portapack_cpld_jtag_reset();

while(true) {
	led_toggle(LED1);
	delay(10000000);
}
*/
#ifdef CPLD_PROGRAM
	cpu_clock_pll1_max_speed();
	portapack_cpld_jtag_io_init();
	const bool success = portapack_cpld_jtag_program();

	while(true) {
		if( success ) {
			led_toggle(LED1);
		}
		delay(10000000);
	}
#endif

	portapack_driver_init();

#ifdef LCD_BACKLIGHT_TEST
	cpu_clock_pll1_max_speed();
	while(true) {
		led_on(LED1);
		portapack_lcd_backlight(1);
		delay(10000000);
		led_off(LED1);
		portapack_lcd_backlight(0);
		delay(10000000);
	}
#endif
	
	portapack_init();

	ssp1_init();
/*
	usb_set_configuration_changed_cb(usb_configuration_changed);
	usb_peripheral_reset();
	
	usb_device_init(0, &usb_device);
	
	usb_queue_init(&usb_endpoint_control_out_queue);
	usb_queue_init(&usb_endpoint_control_in_queue);
	usb_queue_init(&usb_endpoint_bulk_out_queue);
	usb_queue_init(&usb_endpoint_bulk_in_queue);

	usb_endpoint_init(&usb_endpoint_control_out);
	usb_endpoint_init(&usb_endpoint_control_in);

	nvic_set_priority(NVIC_USB0_IRQ, 255);

	usb_run(&usb_device);
*/
/*
	while(true) {
		led_toggle(LED1);
		delay(10000000);
	}
*/
	while(true) {
		portapack_run();
	}

	return 0;
}
