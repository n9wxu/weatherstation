#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/rtc.h"
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "rain_task.h"
#include "wind_task.h"
#include "gps_task.h"
#include "reporting_task.h"
#include "temperature_task.h"
#include "pressure_task.h"

#include "switch_inputs.pio.h"

const int wind_speed_pin = 27;
const int rain_bucket_pin = 9;

const int wind_led = 8;
const int rain_led = 7;
const int gps_led = 3;
const int bmp_led = 2;
const int tmp_led = 1;
const int report_led = 0;

PIO pio;

static int8_t pio_irq;

int main()
{
	stdio_init_all();
	rtc_init();

	gpio_init(wind_speed_pin);
	gpio_init(rain_bucket_pin);
	gpio_set_pulls(wind_speed_pin, false, false);
	gpio_set_pulls(rain_bucket_pin, false, false);

	pio = pio0;
	wind_sm = pio_claim_unused_sm(pio, false);
	rain_sm = pio_claim_unused_sm(pio, false);

	pio_sm_set_enabled(pio, wind_sm, false);
	pio_sm_set_enabled(pio, rain_sm, false);
	int offset = pio_add_program(pio, &input_program);
	input_program_init(pio, wind_sm, offset, wind_led, wind_speed_pin);
	input_program_init(pio, rain_sm, offset, rain_led, rain_bucket_pin);
	pio_sm_set_enabled(pio, wind_sm, true);
	pio_sm_set_enabled(pio, rain_sm, true);

	pio_irq = PIO0_IRQ_0;
	irq_get_exclusive_handler(pio_irq);
	irq_add_shared_handler(pio_irq, wind_irq_func, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
	irq_set_enabled(pio_irq, true);
	pio_set_irqn_source_enabled(pio, 0, pis_sm0_rx_fifo_not_empty + wind_sm, true);

	pio_irq = PIO0_IRQ_1;
	irq_get_exclusive_handler(pio_irq);
	irq_add_shared_handler(pio_irq, rain_irq_func, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
	irq_set_enabled(pio_irq, true);
	pio_set_irqn_source_enabled(pio, 1, pis_sm0_rx_fifo_not_empty + rain_sm, true);

	init_reporting();
	init_rain();
	init_wind();
	init_gps();
	init_temperature();
	init_pressure();

	vTaskStartScheduler();
}

void putGPSLED(bool v)
{
	static bool initialized = false;
	if (!initialized)
	{
		gpio_init(gps_led);
		gpio_set_dir(gps_led, true);
		initialized = true;
	}
	gpio_put(gps_led, v);
}

void putBMPLED(bool v)
{
	static bool initialized = false;
	if (!initialized)
	{
		gpio_init(bmp_led);
		gpio_set_dir(bmp_led, true);
		initialized = true;
	}
	gpio_put(bmp_led, v);
}

void putTMPLED(bool v)
{
	static bool initialized = false;
	if (!initialized)
	{
		gpio_init(tmp_led);
		gpio_set_dir(tmp_led, true);
		initialized = true;
	}
	gpio_put(tmp_led, v);
}

void putRPTLED(bool v)
{
	static bool initialized = false;
	if (!initialized)
	{
		gpio_init(report_led);
		gpio_set_dir(report_led, true);
		initialized = true;
	}
	gpio_put(report_led, v);
}
