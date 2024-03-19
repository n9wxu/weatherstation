#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/rtc.h"

#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "rain_task.h"
#include "wind_task.h"
#include "gps_task.h"
#include "reporting_task.h"
#include "temperature_task.h"
#include "pressure_task.h"

#include "switch_inputs.pio.h"

#include "i2c_support.h"

#include "pinmap.h"

PIO pio;

static int8_t pio_irq;

int main()
{
	stdio_init_all();

	i2c_sensorInit();

	gpio_init(WIND_SPEED_PIN);
	gpio_init(RAIN_BUCKET_PIN);
	gpio_pull_up(WIND_SPEED_PIN);
	gpio_pull_up(RAIN_BUCKET_PIN);

	init_reporting();
	init_rain();
	init_wind();
	init_gps();
	init_temperature();
	init_pressure();

	pio = pio0;
	wind_sm = pio_claim_unused_sm(pio, false);
	rain_sm = pio_claim_unused_sm(pio, false);

	pio_sm_set_enabled(pio, wind_sm, false);
	pio_sm_set_enabled(pio, rain_sm, false);
	int offset = pio_add_program(pio, &input_program);
	input_program_init(pio, wind_sm, offset, WIND_LED_PIN, WIND_SPEED_PIN);
	input_program_init(pio, rain_sm, offset, RAIN_LED_PIN, RAIN_BUCKET_PIN);
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

	vTaskStartScheduler();
}

void putGPSLED(bool v)
{
	static bool initialized = false;
	if (!initialized)
	{
		gpio_init(GPS_LED_PIN);
		gpio_set_dir(GPS_LED_PIN, true);
		initialized = true;
	}
	gpio_put(GPS_LED_PIN, v);
}

void putBMPLED(bool v)
{
	static bool initialized = false;
	if (!initialized)
	{
		gpio_init(BMP_LED_PIN);
		gpio_set_dir(BMP_LED_PIN, true);
		initialized = true;
	}
	gpio_put(BMP_LED_PIN, v);
}

void putTMPLED(bool v)
{
	static bool initialized = false;
	if (!initialized)
	{
		gpio_init(TMP_LED_PIN);
		gpio_set_dir(TMP_LED_PIN, true);
		initialized = true;
	}
	gpio_put(TMP_LED_PIN, v);
}

void putRPTLED(bool v)
{
	static bool initialized = false;
	if (!initialized)
	{
		gpio_init(REPORT_LED_PIN);
		gpio_set_dir(REPORT_LED_PIN, true);
		initialized = true;
	}
	gpio_put(REPORT_LED_PIN, v);
}
