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

#include "portapack_driver.h"

#include <delay.h>
#include <led.h>

#include <libopencm3/lpc43xx/gpio.h>
#include <libopencm3/lpc43xx/i2c.h>
#include <libopencm3/lpc43xx/scu.h>

#include "wm8731.h"

int8_t* const sample_buffer_0 = (int8_t*)0x20008000;
int8_t* const sample_buffer_1 = (int8_t*)0x2000c000;
device_state_t* const device_state = (device_state_t*)0x20007000;
uint8_t* const ipc_m4_buffer = (uint8_t*)0x20007f00;
uint8_t* const ipc_m0_buffer = (uint8_t*)0x20007e00;
const size_t ipc_m4_buffer_size = 256;
const size_t ipc_m0_buffer_size = 256;

#define PORTAPACK_SDIO_CD_SCU_PIN (P1_13)
#define PORTAPACK_SDIO_CD_SCU_FUNCTION (SCU_CONF_FUNCTION7)
#define PORTAPACK_SDIO_DAT0_SCU_PIN (P1_9)
#define PORTAPACK_SDIO_DAT0_SCU_FUNCTION (SCU_CONF_FUNCTION7)
#define PORTAPACK_SDIO_DAT1_SCU_PIN (P1_10)
#define PORTAPACK_SDIO_DAT1_SCU_FUNCTION (SCU_CONF_FUNCTION7)
#define PORTAPACK_SDIO_DAT2_SCU_PIN (P1_11)
#define PORTAPACK_SDIO_DAT2_SCU_FUNCTION (SCU_CONF_FUNCTION7)
#define PORTAPACK_SDIO_DAT3_SCU_PIN (P1_12)
#define PORTAPACK_SDIO_DAT3_SCU_FUNCTION (SCU_CONF_FUNCTION7)
#define PORTAPACK_SDIO_CLK_SCU_PIN (CLK0)
#define PORTAPACK_SDIO_CLK_SCU_FUNCTION (SCU_CONF_FUNCTION4)
#define PORTAPACK_SDIO_CMD_SCU_PIN (P1_6)
#define PORTAPACK_SDIO_CMD_SCU_FUNCTION (SCU_CONF_FUNCTION7)
#define PORTAPACK_SDIO_POW_SCU_PIN (P1_5)
#define PORTAPACK_SDIO_POW_SCU_FUNCTION (SCU_CONF_FUNCTION7)

#define PORTAPACK_I2S0_TX_MCLK_SCU_PIN (CLK2)
#define PORTAPACK_I2S0_TX_MCLK_SCU_FUNCTION SCU_CONF_FUNCTION6
#define PORTAPACK_I2S0_TX_SCK_SCU_PIN (P3_0)
#define PORTAPACK_I2S0_TX_SCK_SCU_FUNCTION SCU_CONF_FUNCTION2
#define PORTAPACK_I2S0_TX_WS_SCU_PIN (P3_1)
#define PORTAPACK_I2S0_TX_WS_SCU_FUNCTION SCU_CONF_FUNCTION0
#define PORTAPACK_I2S0_TX_SDA_SCU_PIN (P3_2)
#define PORTAPACK_I2S0_TX_SDA_SCU_FUNCTION SCU_CONF_FUNCTION0
/* NOTE: I2S0_RX_SDA shared with CPLD_TDI */
#define PORTAPACK_I2S0_RX_SDA_SCU_PIN (P6_2)
#define PORTAPACK_I2S0_RX_SDA_SCU_FUNCTION SCU_CONF_FUNCTION3

#define PORTAPACK_DATA_PORT (GPIO3)
#define PORTAPACK_DATA_LSB_PIN (8)
#define PORTAPACK_DATA_MASK (0xffUL << PORTAPACK_DATA_LSB_PIN)

#define PORTAPACK_DATA_DATA0_SCU_PIN (P7_0)
#define PORTAPACK_DATA_DATA0_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_DATA_DATA1_SCU_PIN (P7_1)
#define PORTAPACK_DATA_DATA1_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_DATA_DATA2_SCU_PIN (P7_2)
#define PORTAPACK_DATA_DATA2_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_DATA_DATA3_SCU_PIN (P7_3)
#define PORTAPACK_DATA_DATA3_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_DATA_DATA4_SCU_PIN (P7_4)
#define PORTAPACK_DATA_DATA4_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_DATA_DATA5_SCU_PIN (P7_5)
#define PORTAPACK_DATA_DATA5_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_DATA_DATA6_SCU_PIN (P7_6)
#define PORTAPACK_DATA_DATA6_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_DATA_DATA7_SCU_PIN (P7_7)
#define PORTAPACK_DATA_DATA7_SCU_FUNCTION (SCU_CONF_FUNCTION0)

/*
MODE	DIR	STROBE	ADDR	Description
0		0	_/		0		IO write, MCU -> TP_EN[3:0], TP_D[3:0]
0		0	_/		1		IO write, MCU[1:0] -> LCD_BACKLIGHT, LCD_RESET
0		1	X		X		IO read, switches -> MCU
1		0	\_				LCD write, MCU -> LCD_DB[15:8]
1		0	_/		LCD_RS	LCD write, MCU -> LCD_DB[7:0], ADDR -> LCD_RS
1		1	0				LCD read, LCD_DB[15:8] -> MCU
1		1	_/				LCD read, LCD_DB[7:0] -> MCU
*/
/*
#define PORTAPACK_CONTROL_GPIO_PORT (GPIO5)
#define PORTAPACK_CONTROL_
#define PORTAPACK_CONTROL_MASK ()
*/
#define PORTAPACK_BACKLIGHT_SCU_PIN (P2_8)
#define PORTAPACK_BACKLIGHT_SCU_FUNCTION (SCU_CONF_FUNCTION4)
#define PORTAPACK_BACKLIGHT_GPIO_PORT (GPIO5)
#define PORTAPACK_BACKLIGHT_GPIO_PIN (7)
#define PORTAPACK_BACKLIGHT_GPIO_BIT (1UL << PORTAPACK_BACKLIGHT_GPIO_PIN)
#define PORTAPACK_BACKLIGHT_GPIO_W GPIO_W(5, PORTAPACK_BACKLIGHT_GPIO_PIN)

#define PORTAPACK_DIR_SCU_PIN (P2_13)
#define PORTAPACK_DIR_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_DIR_GPIO_PORT (GPIO1)
#define PORTAPACK_DIR_GPIO_PIN (13)
#define PORTAPACK_DIR_GPIO_BIT (1UL << PORTAPACK_DIR_GPIO_PIN)
#define PORTAPACK_DIR_GPIO_W GPIO_W(1, PORTAPACK_DIR_GPIO_PIN)

#define PORTAPACK_STROBE_SCU_PIN (P2_9)
#define PORTAPACK_STROBE_SCU_FUNCTION (SCU_CONF_FUNCTION0)
#define PORTAPACK_STROBE_GPIO_PORT (GPIO1)
#define PORTAPACK_STROBE_GPIO_PIN (10)
#define PORTAPACK_STROBE_GPIO_BIT (1UL << PORTAPACK_STROBE_GPIO_PIN)
#define PORTAPACK_STROBE_GPIO_W GPIO_W(1, PORTAPACK_STROBE_GPIO_PIN)

#define PORTAPACK_ROT_B_SCU_PIN (P2_4)
#define PORTAPACK_ROT_B_SCU_FUNCTION (SCU_CONF_FUNCTION4)
#define PORTAPACK_ROT_B_GPIO_PORT (GPIO5)
#define PORTAPACK_ROT_B_GPIO_PORT_NUM (5)
#define PORTAPACK_ROT_B_GPIO_PIN (4)
#define PORTAPACK_ROT_B_GPIO_BIT (1UL << PORTAPACK_ROT_B_GPIO_PIN)
#define PORTAPACK_ROT_B_GPIO_W GPIO_W(PORTAPACK_ROT_B_GPIO_PORT_NUM, PORTAPACK_ROT_B_GPIO_PIN)

#define PORTAPACK_ROT_A_SCU_PIN (P2_3)
#define PORTAPACK_ROT_A_SCU_FUNCTION (SCU_CONF_FUNCTION4)
#define PORTAPACK_ROT_A_GPIO_PORT (GPIO5)
#define PORTAPACK_ROT_A_GPIO_PORT_NUM (5)
#define PORTAPACK_ROT_A_GPIO_PIN (3)
#define PORTAPACK_ROT_A_GPIO_BIT (1UL << PORTAPACK_ROT_A_GPIO_PIN)
#define PORTAPACK_ROT_A_GPIO_W GPIO_W(PORTAPACK_ROT_A_GPIO_PORT_NUM, PORTAPACK_ROT_A_GPIO_PIN)

#define PORTAPACK_MODE_SCU_PIN (P2_0)
#define PORTAPACK_MODE_SCU_FUNCTION (SCU_CONF_FUNCTION4)
#define PORTAPACK_MODE_GPIO_PORT (GPIO5)
#define PORTAPACK_MODE_GPIO_PIN (0)
#define PORTAPACK_MODE_GPIO_BIT (1UL << PORTAPACK_MODE_GPIO_PIN)
#define PORTAPACK_MODE_GPIO_W GPIO_W(5, PORTAPACK_MODE_GPIO_PIN)

#define PORTAPACK_ADDR_SCU_PIN (P2_1)
#define PORTAPACK_ADDR_SCU_FUNCTION (SCU_CONF_FUNCTION4)
#define PORTAPACK_ADDR_GPIO_PORT (GPIO5)
#define PORTAPACK_ADDR_GPIO_PIN (1)
#define PORTAPACK_ADDR_GPIO_BIT (1UL << PORTAPACK_ADDR_GPIO_PIN)
#define PORTAPACK_ADDR_GPIO_W GPIO_W(5, PORTAPACK_ADDR_GPIO_PIN)

#define LCD_TOUCH_YP_OE (1 << 7)
#define LCD_TOUCH_YN_OE (1 << 6)
#define LCD_TOUCH_XP_OE (1 << 5)
#define LCD_TOUCH_XN_OE (1 << 4)
#define LCD_TOUCH_YP_OUT_1 (LCD_TOUCH_YP_OE | (1 << 3))
#define LCD_TOUCH_YP_OUT_0 (LCD_TOUCH_YP_OE | (0 << 3))
#define LCD_TOUCH_YN_OUT_1 (LCD_TOUCH_YN_OE | (1 << 2))
#define LCD_TOUCH_YN_OUT_0 (LCD_TOUCH_YN_OE | (0 << 2))
#define LCD_TOUCH_XP_OUT_1 (LCD_TOUCH_XP_OE | (1 << 1))
#define LCD_TOUCH_XP_OUT_0 (LCD_TOUCH_XP_OE | (0 << 1))
#define LCD_TOUCH_XN_OUT_1 (LCD_TOUCH_XN_OE | (1 << 0))
#define LCD_TOUCH_XN_OUT_0 (LCD_TOUCH_XN_OE | (0 << 0))

static void portapack_dir_write() {
	if( PORTAPACK_DIR_GPIO_W != 0 ) {
		// CPLD: input
		PORTAPACK_DIR_GPIO_W = 0;
		// MCU: output
		GPIO_DIR(PORTAPACK_DATA_PORT) |= PORTAPACK_DATA_MASK;
	}
}

static void portapack_dir_read() {
	if( PORTAPACK_DIR_GPIO_W == 0 ) {
		// MCU: input
		GPIO_DIR(PORTAPACK_DATA_PORT) &= ~PORTAPACK_DATA_MASK;
		// CPLD: output
		PORTAPACK_DIR_GPIO_W = 1;
	}
}

static void portapack_mode_io() {
	PORTAPACK_MODE_GPIO_W = 0;
}

static void portapack_mode_lcd() {
	PORTAPACK_MODE_GPIO_W = 1;
}

static void portapack_strobe(const uint32_t value) {
	PORTAPACK_STROBE_GPIO_W = value;
}

static void portapack_addr(const uint32_t value) {
	PORTAPACK_ADDR_GPIO_W = value;
}

void portapack_lcd_backlight(const bool on) {
	PORTAPACK_BACKLIGHT_GPIO_W = on;
}

static void portapack_data_write(const uint32_t value) {
	GPIO_MPIN(PORTAPACK_DATA_PORT) = (value << PORTAPACK_DATA_LSB_PIN);
}

static uint32_t portapack_data_read() {
	return GPIO_MPIN(PORTAPACK_DATA_PORT) >> PORTAPACK_DATA_LSB_PIN;
}
#if 0
void portapack_noise_test(const unsigned int n) {
	portapack_data_write(0);
	portapack_dir_write();
	portapack_mode_lcd();
	portapack_addr(1);
	portapack_strobe(1);

	// 149.075 MHz

	// 185.45, 166.9, 157.625 (x9.275)
	// STROBE: 185.45, 3/10
	// ADDR toggle: 185.450, 3/10
	// MODE: 185.45
	// DIR: 170MHz, 7/10.
	for(unsigned int i=0; i<n; i++) {
		//portapack_data_write(0xff);
		//PORTAPACK_STROBE_GPIO_W = 0;
		//PORTAPACK_ADDR_GPIO_W = 0;
		PORTAPACK_MODE_GPIO_W = 0;
		//PORTAPACK_DIR_GPIO_W = 0;
		__asm__("nop");
		__asm__("nop");
		__asm__("nop");
		__asm__("nop");
		__asm__("nop");
		//portapack_data_write(0x00);
		//PORTAPACK_STROBE_GPIO_W = 1;
		//PORTAPACK_ADDR_GPIO_W = 1;
		PORTAPACK_MODE_GPIO_W = 1;
		//PORTAPACK_DIR_GPIO_W = 1;
		__asm__("nop");
		__asm__("nop");
		__asm__("nop");
		__asm__("nop");
		__asm__("nop");
	}
}
#endif
static void portapack_io_write(const uint32_t addr, const uint32_t value) {
	portapack_data_write(value);
	portapack_dir_write();
	portapack_mode_io();
	portapack_addr(addr);
	delay(10);
	portapack_strobe(0);
	delay(10);
	portapack_strobe(1);
	delay(10);
}

static uint32_t portapack_io_read() {
	portapack_dir_read();
	portapack_mode_io();
	delay(10);
	return portapack_data_read();
}

void portapack_lcd_write(const uint32_t rs, const uint32_t value) {
	portapack_data_write(value >> 8);
	portapack_dir_write();
	portapack_mode_lcd();
	portapack_addr(rs);
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
	portapack_strobe(0);
	portapack_data_write(value & 0xff);
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
	portapack_strobe(1);
	portapack_addr(1);
}

void portapack_lcd_write_data_fast(const uint32_t value) {
	//portapack_data_write(value >> 8);
	GPIO_MPIN(PORTAPACK_DATA_PORT) = value;
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
	portapack_strobe(0);
	//portapack_data_write(value & 0xff);
	GPIO_MPIN(PORTAPACK_DATA_PORT) = (value << 8);
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
	portapack_strobe(1);
}

void portapack_lcd_frame_sync() {
	while( (portapack_io_read() & 0x80) == 0 );
}

void portapack_lcd_touch_sense_hi_z() {
	/* All signals are allowed to float. */
	portapack_io_write(0, 0);
}

void portapack_lcd_touch_sense_off() {
	/* All signals are driven to same voltage. */
	portapack_io_write(0,
		  LCD_TOUCH_YP_OUT_0
		| LCD_TOUCH_YN_OUT_0
		| LCD_TOUCH_XP_OUT_0
		| LCD_TOUCH_XN_OUT_0
	);
}

void portapack_lcd_touch_sense_pressure() {
	portapack_io_write(0, LCD_TOUCH_XN_OUT_0 | LCD_TOUCH_YP_OUT_1);
}

void portapack_lcd_touch_sense_x() {
	// X signals are driven to opposite potentials.
	// Read voltage from Y signals.
	portapack_io_write(0, LCD_TOUCH_XP_OUT_1 | LCD_TOUCH_XN_OUT_0);
}

void portapack_lcd_touch_sense_y() {
	// Y signals are driven to opposite potentials.
	// Read voltage from X signals.
	portapack_io_write(0, LCD_TOUCH_YP_OUT_1 | LCD_TOUCH_YN_OUT_0);
}

uint32_t portapack_read_switches() {
	return portapack_io_read();
}

static volatile uint_fast8_t portapack_io_config_value = 0;

void portapack_lcd_reset(const bool active) {
	if( active ) {
		portapack_io_config_value |= 0x01;
	} else {
		portapack_io_config_value &= ~0x01;
	}
	portapack_io_write(1, portapack_io_config_value);
}

static uint32_t portapack_encoder_phase_0() {
	return PORTAPACK_ROT_A_GPIO_W & 1;
}

static uint32_t portapack_encoder_phase_1() {
	return PORTAPACK_ROT_B_GPIO_W & 1;
}

static uint_fast8_t encoder_state = 0;

static const int8_t encoder_transition_map[] = {
	 0,	// 0000: noop
	 0,	// 0001: start
	 0,	// 0010: start
	 0,	// 0011: rate
	 1,	// 0100: end
	 0,	// 0101: noop
	 0,	// 0110: rate
	-1,	// 0111: end
	-1,	// 1000: end
	 0,	// 1001: rate
	 0,	// 1010: noop
	 1,	// 1011: end
	 0,	// 1100: rate
	 0,	// 1101: start
	 0,	// 1110: start
	 0,	// 1111: noop
};

int encoder_update() {
	encoder_state <<= 1;
	const uint32_t phase_0 = portapack_encoder_phase_0();
	encoder_state |= phase_0;
	encoder_state <<= 1;
	const uint32_t phase_1 = portapack_encoder_phase_1();
	encoder_state |= phase_1;

	const int32_t result = encoder_transition_map[encoder_state & 0xf];
	device_state->encoder_position += result;

	return result;
}

#if (defined ENCODER_USE_INTERRUPTS || defined LPC43XX_M4)
#include <libopencm3/lpc43xx/m4/nvic.h>
#endif

void portapack_encoder_init() {
	device_state->encoder_position = 0;

#if (defined ENCODER_USE_INTERRUPTS || defined LPC43XX_M4)
	/* PortaPack rotary encoder is read from the M4, since the M0 can't
	 * receive multiple GPIO pin interrupts, and polling would be a pain
	 * if done properly (fast), or would work poorly if done slowly.
	 */
	SCU_PINTSEL0 =
		  (PORTAPACK_ROT_B_GPIO_PORT_NUM << 13)
		| (PORTAPACK_ROT_B_GPIO_PIN << 8)
		| (PORTAPACK_ROT_A_GPIO_PORT_NUM << 5)
		| (PORTAPACK_ROT_A_GPIO_PIN << 0)
		;

	GPIO_PIN_INTERRUPT_ISEL =
		  (0 << 1)	// 1: edge-sensitive
		| (0 << 0)	// 0: edge-sensitive
		;

	GPIO_PIN_INTERRUPT_IENR =
		  (1 << 1)	// 1: enable rising-edge interrupt
		| (1 << 0) 	// 0: enable rising-edge interrupt
		;

	GPIO_PIN_INTERRUPT_IENF =
		  (1 << 1)	// 1: enable falling-edge interrupt
		| (1 << 0)	// 0: enable falling-edge interrupt
		;

	nvic_set_priority(NVIC_PIN_INT0_IRQ, 255);
	nvic_enable_irq(NVIC_PIN_INT0_IRQ);
	nvic_set_priority(NVIC_PIN_INT1_IRQ, 255);
	nvic_enable_irq(NVIC_PIN_INT1_IRQ);
#endif
}

#if (defined ENCODER_USE_INTERRUPTS || defined LPC43XX_M4)
extern "C" void pin_int0_isr() {
	if( GPIO_PIN_INTERRUPT_IST & (1 << 0) ) {
		encoder_update();
		GPIO_PIN_INTERRUPT_IST = (1 << 0);
		GPIO_PIN_INTERRUPT_RISE = (1 << 0);
		GPIO_PIN_INTERRUPT_FALL = (1 << 0);
	}
}

extern "C" void pin_int1_isr() {
	if( GPIO_PIN_INTERRUPT_IST & (1 << 1) ) {
		encoder_update();
		GPIO_PIN_INTERRUPT_RISE = (1 << 1);
		GPIO_PIN_INTERRUPT_FALL = (1 << 1);
		GPIO_PIN_INTERRUPT_IST = (1 << 1);
	}
}
#endif

void portapack_driver_init() {
	GPIO_MASK(PORTAPACK_DATA_PORT) = ~PORTAPACK_DATA_MASK;

	// Control signals to FPGA: outputs.
	GPIO_DIR(PORTAPACK_MODE_GPIO_PORT) |= PORTAPACK_MODE_GPIO_BIT;
	GPIO_DIR(PORTAPACK_DIR_GPIO_PORT) |= PORTAPACK_DIR_GPIO_BIT;
	GPIO_DIR(PORTAPACK_STROBE_GPIO_PORT) |= PORTAPACK_STROBE_GPIO_BIT;
	GPIO_DIR(PORTAPACK_ADDR_GPIO_PORT) |= PORTAPACK_ADDR_GPIO_BIT;
	GPIO_DIR(PORTAPACK_BACKLIGHT_GPIO_PORT) |= PORTAPACK_BACKLIGHT_GPIO_BIT;
	GPIO_DIR(PORTAPACK_ROT_A_GPIO_PORT) &= ~PORTAPACK_ROT_A_GPIO_BIT;
	GPIO_DIR(PORTAPACK_ROT_B_GPIO_PORT) &= ~PORTAPACK_ROT_B_GPIO_BIT;

	portapack_data_write(0);
	portapack_dir_read();
	portapack_mode_io();
	portapack_strobe(1);
	portapack_addr(0);
	portapack_lcd_backlight(0);

	scu_pinmux(PORTAPACK_MODE_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_MODE_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_DIR_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DIR_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_STROBE_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_STROBE_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_ADDR_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_ADDR_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_BACKLIGHT_SCU_PIN, (SCU_GPIO_PUP | PORTAPACK_BACKLIGHT_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_ROT_A_SCU_PIN, SCU_GPIO_NOPULL | PORTAPACK_ROT_A_SCU_FUNCTION);
	scu_pinmux(PORTAPACK_ROT_B_SCU_PIN, SCU_GPIO_NOPULL | PORTAPACK_ROT_B_SCU_FUNCTION);

	scu_pinmux(PORTAPACK_DATA_DATA0_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DATA_DATA0_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_DATA_DATA1_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DATA_DATA1_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_DATA_DATA2_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DATA_DATA2_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_DATA_DATA3_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DATA_DATA3_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_DATA_DATA4_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DATA_DATA4_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_DATA_DATA5_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DATA_DATA5_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_DATA_DATA6_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DATA_DATA6_SCU_FUNCTION));
	scu_pinmux(PORTAPACK_DATA_DATA7_SCU_PIN, (SCU_GPIO_NOPULL | PORTAPACK_DATA_DATA7_SCU_FUNCTION));

	/* NOTE: Turn on I2S TX_WS input buffer, or you won't get it back into the
	 * I2S peripheral for RX. */
	scu_pinmux(PORTAPACK_I2S0_TX_MCLK_SCU_PIN, PORTAPACK_I2S0_TX_MCLK_SCU_FUNCTION | SCU_CONF_EPD_EN_PULLDOWN | SCU_CONF_EPUN_DIS_PULLUP);
	scu_pinmux(PORTAPACK_I2S0_TX_SCK_SCU_PIN,  PORTAPACK_I2S0_TX_SCK_SCU_FUNCTION  | SCU_CONF_EPD_EN_PULLDOWN | SCU_CONF_EPUN_DIS_PULLUP);
	scu_pinmux(PORTAPACK_I2S0_TX_WS_SCU_PIN,   PORTAPACK_I2S0_TX_WS_SCU_FUNCTION   | SCU_CONF_EPD_EN_PULLDOWN | SCU_CONF_EPUN_DIS_PULLUP | SCU_CONF_EZI_EN_IN_BUFFER);
	scu_pinmux(PORTAPACK_I2S0_TX_SDA_SCU_PIN,  PORTAPACK_I2S0_TX_SDA_SCU_FUNCTION  | SCU_CONF_EPD_EN_PULLDOWN | SCU_CONF_EPUN_DIS_PULLUP);
	scu_pinmux(PORTAPACK_I2S0_RX_SDA_SCU_PIN,  PORTAPACK_I2S0_RX_SDA_SCU_FUNCTION  | SCU_CONF_EPD_EN_PULLDOWN | SCU_CONF_EPUN_DIS_PULLUP | SCU_CONF_EZI_EN_IN_BUFFER);

	/* At 30MHz, disable input glitch filter. At 50MHz, enable fast output slew rate. */
	const bool fast = false;
	const uint32_t pin_speed = fast ? (SCU_CONF_EHS_FAST | SCU_CONF_ZIF_DIS_IN_GLITCH_FILT) : 0;

	/*	CD			I	PU
	 *	DAT[3:0]	IO	PU
	 *	CLK			IO	-
	 *	CMD			IO	PU
	 */
	scu_pinmux(PORTAPACK_SDIO_CD_SCU_PIN,	(PORTAPACK_SDIO_CD_SCU_FUNCTION   | SCU_CONF_EZI_EN_IN_BUFFER));
	scu_pinmux(PORTAPACK_SDIO_DAT0_SCU_PIN, (PORTAPACK_SDIO_DAT0_SCU_FUNCTION | SCU_CONF_EZI_EN_IN_BUFFER | pin_speed));
	scu_pinmux(PORTAPACK_SDIO_DAT1_SCU_PIN, (PORTAPACK_SDIO_DAT1_SCU_FUNCTION | SCU_CONF_EZI_EN_IN_BUFFER | pin_speed));
	scu_pinmux(PORTAPACK_SDIO_DAT2_SCU_PIN, (PORTAPACK_SDIO_DAT2_SCU_FUNCTION | SCU_CONF_EZI_EN_IN_BUFFER | pin_speed));
	scu_pinmux(PORTAPACK_SDIO_DAT3_SCU_PIN, (PORTAPACK_SDIO_DAT3_SCU_FUNCTION | SCU_CONF_EZI_EN_IN_BUFFER | pin_speed));
	scu_pinmux(PORTAPACK_SDIO_CLK_SCU_PIN,	(PORTAPACK_SDIO_CLK_SCU_FUNCTION  | SCU_CONF_EPUN_DIS_PULLUP  | pin_speed));
	scu_pinmux(PORTAPACK_SDIO_CMD_SCU_PIN,	(PORTAPACK_SDIO_CMD_SCU_FUNCTION  | SCU_CONF_EZI_EN_IN_BUFFER | pin_speed));

	portapack_lcd_touch_sense_off();
}

void portapack_audio_codec_write(const uint_fast8_t address, const uint_fast16_t data) {
	uint16_t word = (address << 9) | data;
	i2c0_tx_start();
	i2c0_tx_byte(WM8731_I2C_ADDR | I2C_WRITE);
	i2c0_tx_byte(word >> 8);
	i2c0_tx_byte(word & 0xff);
	i2c0_stop();
}
