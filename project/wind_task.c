#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hardware/pio.h"
#include "hardware/adc.h"

#include <stdio.h>
#include <pinmap.h>
#include "reporting_task.h"
#include "stdlib.h"

static QueueHandle_t windQueue;
unsigned int wind_sm;

extern PIO pio;

#define WIND_PRIORITY 30

void wind_irq_func(void)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    uint32_t c; // empty the fifo and keep the last value
    while (!pio_sm_is_rx_fifo_empty(pio, wind_sm))
    {
        c = pio->rxf[wind_sm];
    }
    xQueueSendFromISR(windQueue, &c, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

struct dir_counts_s
{
    uint32_t counts;
    int degrees;
};
// pointed the arrow in each direction and recorded the counts
const struct dir_counts_s direction_map[] = {
    {4400, 0},
    {1400, 45},
    {420, 90},
    {600, 135},
    {840, 180},
    {2400, 225},
    {12600, 270},
    {7900, 315}};

int measureDirection()
{
    uint32_t counts = 0;
    adc_select_input(WIND_DIR_ADC);
    // 4-bit oversample to increase resolution and decrease noise
    for (int ovr_sample = 0; ovr_sample < 256; ovr_sample++)
    {
        counts += adc_read();
    }
    counts /= 16;
    int closest_index = 0; // assume the first index is the closest
    // find closest counts in the map
    for (int map_index = 0; map_index < sizeof(direction_map) / sizeof(*direction_map); map_index++)
    {
        int difference = abs(counts - direction_map[map_index].counts);
        int closest_difference = abs(counts - direction_map[closest_index].counts);
        if (difference < closest_difference)
        {
            closest_index = map_index;
        }
    }

    return direction_map[closest_index].degrees;
}

static void wind_task(void *parameter)
{
    int count = 0;
    int lastTransmission = 0;
    for (;;)
    {
        xQueueReceive(windQueue, &count, pdMS_TO_TICKS(250)); // wait up to 250ms for counts data
        int now = xTaskGetTickCount() / portTICK_RATE_MS;     // get the up-time in ms
        if (now - lastTransmission > 1000)                    // if 1sec has elapsed since our last transmission
        {
            lastTransmission = now;
            int direction = measureDirection(); // collect the current wind direction
            reportWINDData(count, direction);   // send the data
        }
    }
}

void init_wind()
{
    windQueue = xQueueCreate(10, sizeof(int));

    assert(windQueue);
    adc_init();
    adc_gpio_init(WIND_DIR_PIN);

    xTaskCreate(wind_task, "Wind", 1000, NULL, WIND_PRIORITY, NULL);
}
