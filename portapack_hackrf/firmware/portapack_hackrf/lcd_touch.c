
#include "lcd_touch.h"

#define TOUCH_YP_ADC0_INPUT (0)
#define TOUCH_YN_ADC0_INPUT (2)
#define TOUCH_XP_ADC0_INPUT (5)
#define TOUCH_XN_ADC0_INPUT (6)

#include <libopencm3/lpc43xx/adc.h>

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

#include <libopencm3/lpc43xx/m0/nvic.h>

#include <stddef.h>

#include "portapack_driver.h"

void lcd_touch_init() {
	portapack_lcd_touch_sense_off();

	/* Kingtech LCD is 330 Ohms in X direction, 600 Ohms in Y direction
	 * With 3.3V across the touch screen, current consumption can be 10mA
	 * or more. So minimize time on.
	 */
	ADC0_CR =
		  ADC_CR_SEL(
		  	  (1 << TOUCH_XP_ADC0_INPUT)
		  	| (1 << TOUCH_XN_ADC0_INPUT)
		  	| (1 << TOUCH_YP_ADC0_INPUT)
		  	| (1 << TOUCH_YN_ADC0_INPUT)
		  )
		| ADC_CR_CLKDIV(47)	/* 204MHz / 48 = 4.25 MHz */
		| ADC_CR_BURST(0)
		| ADC_CR_CLKS(ADC_CR_CLKS_11_10_BITS)
		| ADC_CR_PDN(ADC_CR_PDN_OPERATIONAL)
		| ADC_CR_START(ADC_CR_START_NO_START)
		| ADC_CR_EDGE(ADC_CR_EDGE_RISING)
		;
}

static uint_fast16_t sample_adc_channel(const uint_fast8_t channel_number) {
	ADC0_CR =
		  ADC_CR_SEL(
		  	  (1 << channel_number)
		  )
		| ADC_CR_CLKDIV(47)	/* 204MHz / 48 = 4.25 MHz */
		| ADC_CR_BURST(0)
		| ADC_CR_CLKS(ADC_CR_CLKS_11_10_BITS)
		| ADC_CR_PDN(ADC_CR_PDN_OPERATIONAL)
		| ADC_CR_START(ADC_CR_START_NOW)
		| ADC_CR_EDGE(ADC_CR_EDGE_RISING)
		;

	while((ADC0_GDR & (1UL << 31)) == 0);

	return (ADC0_GDR >> 6) & 0x3ff;
}

#include "linux_stuff.h"

typedef struct touch_measurements_t {
	uint16_t xp;
	uint16_t xn;
	uint16_t yp;
	uint16_t yn;
} touch_measurements_t;

typedef struct touch_frame_t {
	touch_measurements_t x;
	touch_measurements_t y;
	touch_measurements_t z;
} touch_frame_t;

#define TOUCH_HISTORY_LENGTH 4
static touch_frame_t touch_history[TOUCH_HISTORY_LENGTH];
static uint_fast8_t touch_history_index = 0;

static touch_frame_t touch_values;

static void lcd_touch_measurements_subtract(touch_measurements_t* const a, touch_measurements_t* const b) {
	a->xp -= b->xp;
	a->xn -= b->xn;
	a->yp -= b->yp;
	a->yn -= b->yn;
}

static void lcd_touch_measurements_add(touch_measurements_t* const a, touch_measurements_t* const b) {
	a->xp += b->xp;
	a->xn += b->xn;
	a->yp += b->yp;
	a->yn += b->yn;
}

void lcd_touch_convert(touch_state_t* const state) {
	/* Touch screen references:
	 * http://www.cypress.com/?docID=33540
	 */
	
 	const uint32_t vref_mv = 3300;
	const uint32_t adc_bits = 10;
	const uint32_t adc_count = 1 << adc_bits;
	const uint32_t x_ohm = 330;
	const uint32_t y_ohm = 600;

	portapack_lcd_touch_sense_pressure();

	touch_history_index = (touch_history_index + 1) % TOUCH_HISTORY_LENGTH;
	touch_frame_t* const frame = &touch_history[touch_history_index];

	/* Scan order does low-impedance signals first, to allow other signals to settle. */
	lcd_touch_measurements_subtract(&touch_values.z, &frame->z);
	frame->z.xn = sample_adc_channel(TOUCH_XN_ADC0_INPUT);
	frame->z.yp = sample_adc_channel(TOUCH_YP_ADC0_INPUT);
	frame->z.xp = sample_adc_channel(TOUCH_XP_ADC0_INPUT);
	frame->z.yn = sample_adc_channel(TOUCH_YN_ADC0_INPUT);
	lcd_touch_measurements_add(&touch_values.z, &frame->z);

	portapack_lcd_touch_sense_off();
	portapack_lcd_touch_sense_x(0);

	//const uint32_t z_mv = max(touch_values.z.yp - touch_values.z.xn, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	const uint32_t z_yp_mv = max(touch_values.z.yp - touch_values.z.yn, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	const uint32_t z_xn_mv = max(touch_values.z.xp - touch_values.z.xn, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	const uint32_t z_touch_mv = max(touch_values.z.yn - touch_values.z.xp, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);

	lcd_touch_measurements_subtract(&touch_values.x, &frame->x);
	frame->x.xp = sample_adc_channel(TOUCH_XP_ADC0_INPUT);
	frame->x.xn = sample_adc_channel(TOUCH_XN_ADC0_INPUT);
	frame->x.yn = sample_adc_channel(TOUCH_YN_ADC0_INPUT);
	frame->x.yp = sample_adc_channel(TOUCH_YP_ADC0_INPUT);
	lcd_touch_measurements_add(&touch_values.x, &frame->x);

	portapack_lcd_touch_sense_off();
	portapack_lcd_touch_sense_y(0);

	const uint32_t x_mv = max(touch_values.x.xp - touch_values.x.xn, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	const uint32_t x_ua = x_mv * 1000 / x_ohm;
	const uint32_t xn_mv = max(touch_values.x.yp - touch_values.x.xn, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	const uint32_t xn_mohm = xn_mv * 1000000 / x_ua;
	//const uint32_t xp_mv = max(touch_values.x.xp - touch_values.x.yp, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	//const uint32_t xp_mohm = xp_mv * 1000000 / x_ua;

	lcd_touch_measurements_subtract(&touch_values.y, &frame->y);
	frame->y.yp = sample_adc_channel(TOUCH_YP_ADC0_INPUT);
	frame->y.yn = sample_adc_channel(TOUCH_YN_ADC0_INPUT);
	frame->y.xn = sample_adc_channel(TOUCH_XN_ADC0_INPUT);
	frame->y.xp = sample_adc_channel(TOUCH_XP_ADC0_INPUT);
	lcd_touch_measurements_add(&touch_values.y, &frame->y);

	portapack_lcd_touch_sense_off();

	const uint32_t y_mv = max(touch_values.y.yp - touch_values.y.yn, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	const uint32_t y_ua = y_mv * 1000 / y_ohm;
	const uint32_t yn_mv = max(touch_values.y.xp - touch_values.y.yn, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	const uint32_t yn_mohm = yn_mv * 1000000 / y_ua;
	const uint32_t yp_mv = max(touch_values.y.yp - touch_values.y.xp, 0) * vref_mv / (adc_count * TOUCH_HISTORY_LENGTH);
	const uint32_t yp_mohm = yp_mv * 1000000 / y_ua;

	const uint32_t z_xn_ua = z_xn_mv * 1000000 / xn_mohm;
	const uint32_t z_yp_ua = z_yp_mv * 1000000 / yp_mohm;

	const uint32_t z_touch_ua = (z_xn_ua + z_yp_ua) / 2;

	const uint32_t pressure_threshold_mohm = 1500 * 1000;
	uint32_t pressure = 0;
	if( z_touch_ua > 0 ) {
		const uint32_t z_touch_mohm = z_touch_mv * 1000000 / z_touch_ua;
		if( z_touch_mohm < pressure_threshold_mohm ) {
			pressure = (pressure_threshold_mohm - z_touch_mohm) / 1000;
		}
	}
	state->pressure = pressure;

	/* TODO: Dumb, static calibration code. */
	state->x = max_t(uint32_t, 0, xn_mohm - (x_ohm * 1000 / 11)) * (240 + 53) / (x_ohm * 1000);
	state->y = max_t(uint32_t, 0, yn_mohm - (y_ohm * 1000 / 11)) * (320 + 70) / (y_ohm * 1000);
}
