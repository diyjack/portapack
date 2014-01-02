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

#include <libopencm3/lpc43xx/gpio.h>
#include <libopencm3/lpc43xx/m4/nvic.h>
#include <libopencm3/lpc43xx/rgu.h>

#include <usb.h>
#include <usb_standard_request.h>

#include <usb_device.h>
#include <usb_endpoint.h>
#include <usb_api_board_info.h>
#include <usb_api_cpld.h>
#include <usb_api_register.h>
#include <usb_api_spiflash.h>
#include <usb_api_transceiver.h>

#include "portapack.h"

static const usb_request_handler_fn vendor_request_handler[] = {
	NULL,
	NULL, // set_transceiver_mode
	usb_vendor_request_write_max2837,
	usb_vendor_request_read_max2837,
	usb_vendor_request_write_si5351c,
	usb_vendor_request_read_si5351c,
	usb_vendor_request_set_sample_rate_frac,
	usb_vendor_request_set_baseband_filter_bandwidth,
	usb_vendor_request_write_rffc5071,
	usb_vendor_request_read_rffc5071,
	usb_vendor_request_erase_spiflash,
	usb_vendor_request_write_spiflash,
	usb_vendor_request_read_spiflash,
	NULL, // used to be write_cpld
	usb_vendor_request_read_board_id,
	usb_vendor_request_read_version_string,
	usb_vendor_request_set_freq,
	usb_vendor_request_set_amp_enable,
	usb_vendor_request_read_partid_serialno,
	usb_vendor_request_set_lna_gain,
	usb_vendor_request_set_vga_gain,
	usb_vendor_request_set_txvga_gain,
	usb_vendor_request_set_if_freq,
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
	/* Reset transceiver to idle state until other commands are received */
	if( device->configuration->number == 1 ) {
		// transceiver configuration
		gpio_set(PORT_LED1_3, PIN_LED1);
	} else if( device->configuration->number == 2 ) {
		// CPLD update configuration
		usb_endpoint_init(&usb_endpoint_bulk_out);
		start_cpld_update = true;
	} else {
		/* Configuration number equal 0 means usb bus reset. */
		gpio_clear(PORT_LED1_3, PIN_LED1);
	}
}

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
	cpu_clock_init();
/*
	usb_set_configuration_changed_cb(usb_configuration_changed);
	usb_peripheral_reset();
	
	usb_device_init(0, &usb_device);
	
	usb_queue_init(&usb_endpoint_control_out_queue);
	usb_queue_init(&usb_endpoint_control_in_queue);
	
	usb_endpoint_init(&usb_endpoint_control_out);
	usb_endpoint_init(&usb_endpoint_control_in);
	
	nvic_set_priority(NVIC_USB0_IRQ, 255);

	usb_run(&usb_device);
*/	
	ssp1_init();

	/////////////////////////////////////////////////////////////

	portapack_init();
	
	while(true) {
		portapack_run();
	}

	return 0;
}
