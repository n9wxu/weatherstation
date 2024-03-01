#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hardware/pio.h"

#include <stdio.h>

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

static void wind_task(void *parameter)
{
    for (;;)
    {
        int count;
        if (xQueueReceive(windQueue, &count, portMAX_DELAY) == pdTRUE)
        {
            printf("Wind Count : %d\n", count);
        }
    }
}

void init_wind()
{
    windQueue = xQueueCreate(10, sizeof(int));

    assert(windQueue);

    xTaskCreate(wind_task, "Wind", 1000, NULL, WIND_PRIORITY, NULL);
}
