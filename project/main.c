#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/rtc.h"
#include "hardware/i2c.h"

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

const int wind_speed_pin = 27;
const int rain_bucket_pin = 9;
const int i2c_sda_pin = 20;
const int i2c_sck_pin = 21;

const int wind_led = 8;
const int rain_led = 7;
const int gps_led = 3;
const int bmp_led = 2;
const int tmp_led = 1;
const int report_led = 0;

PIO pio;

static int8_t pio_irq;

SemaphoreHandle_t i2c_semaphore;

#define IC2_SELECTION i2c0
const int I2C_BAUDRATE = 100 * 1000; // 100khz baudrate

int main()
{
	stdio_init_all();

	i2c_init(IC2_SELECTION, I2C_BAUDRATE);
	gpio_set_function(i2c_sck_pin, GPIO_FUNC_I2C);
	gpio_set_function(i2c_sda_pin, GPIO_FUNC_I2C);
	gpio_pull_up(i2c_sck_pin);
	gpio_pull_up(i2c_sda_pin);

	gpio_init(wind_speed_pin);
	gpio_init(rain_bucket_pin);
	gpio_pull_up(wind_speed_pin);
	gpio_pull_up(rain_bucket_pin);

	i2c_semaphore = xSemaphoreCreateMutex();

	rtc_init();
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

bool take_i2c()
{
	BaseType_t result = xSemaphoreTake(i2c_semaphore, portMAX_DELAY);
	return result == pdTRUE;
}

void give_i2c()
{
	xSemaphoreGive(i2c_semaphore);
}
