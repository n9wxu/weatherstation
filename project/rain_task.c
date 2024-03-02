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
    for (;;)
    {
        int count;
        if (xQueueReceive(rainQueue, &count, portMAX_DELAY) == pdTRUE)
        {
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
