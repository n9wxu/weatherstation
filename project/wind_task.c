#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hardware/pio.h"
#include "hardware/adc.h"

#include <stdio.h>
#include <pinmap.h>
#include "reporting_task.h"
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

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

struct wind_data
{
    int direction;
    float speed;
};

// We need to keep track of the following variables:
// Wind speed/dir each update (no storage)
// Wind gust/dir over the day (no storage)
// Wind speed/dir, avg over 2 minutes (store 1 per second)
// Wind gust/dir over last 10 minutes (store 1 per minute)
// Rain over the past hour (store 1 per minute)
// Total rain over date (store one per day)

static void wind_task(void *parameter)
{
    bool transmitRawData = false;
    int seconds_2m = 0;
    int minutes_10m = 0;
    unsigned int lastWindCheck = 0;
    unsigned int count = 0;
    unsigned int lastCounts = 0;
    unsigned int lastUpdate = 0;
    unsigned int lastRawTransmission = 0;
    int logIndex = 0;
    int seconds = 0;
    int minutes = 0;
    struct wind_data windavg_2m[120];  // two minutes of data for every second
    struct wind_data windgust_10m[10]; // last 10 minutes of wind gusts
    struct wind_data windgust;         // daily gust data
    struct wind_data windavg2m;
    struct wind_data gust_10m;

    for (;;)
    {
        xQueueReceive(windQueue, &count, pdMS_TO_TICKS(250));      // wait up to 250ms for counts data
        unsigned int now = xTaskGetTickCount() / portTICK_RATE_MS; // get the up-time in ms
        int currentDirection = measureDirection();                 // collect the current wind direction

        if (now - lastUpdate > 1000) // update the processed data every second.
        {
            lastUpdate += 1000;

            if (++seconds_2m > 119)
                seconds_2m = 0;

            float deltaTime = (float)(lastWindCheck - now) / 1000.0;
            lastWindCheck = now;

            /* care care of the count rollover since the PIO code is a monotonic count */
            uint32_t deltaCounts = count - lastCounts;
            if (deltaCounts < lastCounts)
                deltaCounts += UINT_MAX;
            /* scale into actual MPH wind speed */
            float currentSpeed = 1.492 * ((float)deltaCounts) / deltaTime;
            lastCounts = count;

            windavg_2m[seconds].speed = currentSpeed;
            windavg_2m[seconds].direction = currentDirection;

            if (currentSpeed > windgust_10m[minutes_10m].speed)
            {
                windgust_10m[minutes_10m].speed = currentSpeed;
                windgust_10m[minutes_10m].direction = currentDirection;
            }

            if (currentSpeed > windgust.speed)
            {
                windgust.speed = currentSpeed;
                windgust.direction = currentDirection;
            }

            if (++seconds > 59)
            {
                seconds = 0;
                transmitRawData = true;

                if (++minutes > 59)
                {
                    minutes = 0;
                }
                if (++minutes_10m > 9)
                    minutes_10m = 0;

                windgust_10m[minutes_10m].speed = 0; // Zero out this minute's gust
            }

            // computing the average speed & direction
            {
                float avgspeed_2m = 0;
                int sum = windavg_2m[0].direction;
                int D = sum;
                for (int x = 0; x < 120; x++)
                {
                    int delta = windavg_2m[x].direction - D;
                    if (delta < -180)
                    {
                        D += delta + 360;
                    }
                    else if (delta > 180)
                    {
                        D += delta - 360;
                    }
                    else
                    {
                        D += delta;
                    }
                    sum += D;
                    avgspeed_2m += windavg_2m[x].speed;
                }
                windavg2m.speed = avgspeed_2m / 120.0;
                windavg2m.direction = sum / 120;
                if (windavg2m.direction >= 360)
                    windavg2m.direction -= 360;
                if (windavg2m.direction < 0)
                    windavg2m.direction += 360;
            }

            // calculate the 10 minute gust speed & direction
            {
                gust_10m.direction = 0;
                gust_10m.speed = 0.0;

                for (int i = 0; i < 10; i++)
                {
                    if (windgust_10m[i].speed > gust_10m.speed)
                    {
                        gust_10m.speed = windgust_10m[i].speed;
                        gust_10m.direction = windgust_10m[i].direction;
                    }
                }
            }

            if (transmitRawData) // raw data every minute
            {
                transmitRawData = false;
                reportWINDData(count, currentDirection); // send the data
                reportWINDScaledData(windavg2m.speed, windavg2m.direction, gust_10m.speed, gust_10m.direction);
            }
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
