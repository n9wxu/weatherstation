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

uint32_t convertPin(int pin)
{
    assert(pin >= 26 && pin <= 29);
    int adc_channel = pin - 26;
    printf("starting ADC read of pin %d and adc %d\n", pin, adc_channel);
    adc_init();
    adc_gpio_init(pin);
    adc_select_input(adc_channel);
    adc_fifo_setup(false, false, 0, false, false);
    // discard at least 3 samples until fifo is empty
    int ignore_count = 3;
    for (int x = 0; x < ignore_count; x++)
    {
        (void)adc_read();
    }

    uint32_t counts = 0;
    for (int x = 0; x < 256; x++)
    {
        uint16_t r = adc_read();
        counts += r;
        printf("[%d]=%6d:%6d\n", x, r, counts);
    }
    counts /= 16;
    printf("adc result %d\n", counts);
    return counts;
}

int measureDirection()
{
    uint32_t counts = convertPin(WIND_DIR_PIN);
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

void measureBattery()
{
    // go ahead and measure the battery voltage here so I don't have to share the ADC with another task.
    float volts = 0;
    uint32_t counts = convertPin(VSYS_PIN);
    counts /= 16; // undo the oversample from the conversion
    volts = ((float)counts * 3.0 * 3.3) / 4096.0;
    if (volts > 5.0)
        volts = 5.0; // one of the test boards has a bad ADC that always reads full scale.
    reportBatteryVoltage(volts);
    printf("Supply Voltage %d : %.2fV\n", counts, volts);
}

// We need to keep track of the following variables:
// Wind speed/dir each update (no storage)
// Wind gust/dir over the day (no storage)
// Wind speed/dir, avg over 2 minutes (store 1 per second)
// Wind gust/dir over last 10 minutes (store 1 per minute)
// Rain over the past hour (store 1 per minute)
// Total rain over date (store one per day)

#define WIND_DATA_UPDATE (1000U)

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
    struct wind_data windavg_2m[120] = {{0.0, 0}};  // two minutes of data for every second
    struct wind_data windgust_10m[10] = {{0.0, 0}}; // last 10 minutes of wind gusts
    struct wind_data windgust;                      // daily gust data
    struct wind_data windavg2m;
    struct wind_data gust_10m;

    for (;;)
    {
        if (pdTRUE == xQueueReceive(windQueue, &count, pdMS_TO_TICKS(60000))) // wait up to 1min for counts data
        {
            unsigned int now = xTaskGetTickCount() / portTICK_RATE_MS; // get the up-time in ms
            int currentDirection = measureDirection();                 // collect the current wind direction

            if (now - lastUpdate > WIND_DATA_UPDATE)
            {
                lastUpdate += WIND_DATA_UPDATE;

                if (++seconds_2m > 119)
                    seconds_2m = 0;

                float deltaTime = (float)(now - lastWindCheck);
                lastWindCheck = now;
                deltaTime /= 1000.0;

                /* care care of the count rollover since the PIO code is a monotonic count */
                uint32_t deltaCounts = count - lastCounts;
                if (deltaCounts > lastCounts)
                    deltaCounts += UINT_MAX;
                /* scale into actual MPH wind speed */
                float currentSpeed = 1.492 * ((float)deltaCounts) / deltaTime;
                lastCounts = count;

                windavg_2m[seconds_2m].speed = currentSpeed;
                windavg_2m[seconds_2m].direction = currentDirection;

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
        else
        {
            puts("No wind data in 1 minute");
        }
        measureBattery();
    }
}

void init_wind()
{
    windQueue = xQueueCreate(10, sizeof(int));

    assert(windQueue);

    xTaskCreate(wind_task, "Wind", 1000, NULL, WIND_PRIORITY, NULL);
}
