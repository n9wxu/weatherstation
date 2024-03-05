#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hardware/pio.h"

#include <stdio.h>
#include "reporting_task.h"

static QueueHandle_t rainQueue;
unsigned int rain_sm;

extern PIO pio;

#define RAIN_PRIORITY 30

void rain_irq_func(void)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    uint32_t c; // empty the fifo and keep the last value
    while (!pio_sm_is_rx_fifo_empty(pio, rain_sm))
    {
        c = pio->rxf[rain_sm];
    }
    xQueueSendFromISR(rainQueue, &c, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void rain_task(void *parameter)
{
    uint32_t hour_start_ticks = 0;
    uint32_t hour_start_tips = 0;
    uint32_t day_start_tips = 0;
    float rainInchesLastHour = 0.0;
    float rainInchesLastDay = 0.0;
    int hours = 0;
    for (;;)
    {
        unsigned int count;
        if (xQueueReceive(rainQueue, &count, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            unsigned int now_s = (xTaskGetTickCount() / portTICK_RATE_MS) / 1000;
            if (now_s - hour_start_ticks > 60000)
            {
                hour_start_ticks += 60000; // add an hour
                if (++hours > 23)
                {
                    hours = 0;
                    int tipsLastDay = count - day_start_tips;
                    if (tipsLastDay < day_start_tips)
                    {
                        tipsLastDay += UINT32_MAX;
                    }
                    rainInchesLastDay = tipsLastDay * 0.011;
                }

                int tipsLastHour = count - hour_start_tips;
                if (tipsLastHour < hour_start_tips)
                {
                    tipsLastHour += UINT32_MAX;
                }
                hour_start_tips = count;

                rainInchesLastHour = tipsLastHour * 0.011;
                reportRainScaledData(rainInchesLastHour, rainInchesLastDay);
            }
            reportRAINData(count);
        }
    }
}

void init_rain()
{
    rainQueue = xQueueCreate(
        5,
        sizeof(int));

    xTaskCreate(rain_task, "Rain", 500, NULL, RAIN_PRIORITY, NULL);
}
