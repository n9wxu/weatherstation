#include "FreeRTOS.h"
#include "task.h"
#include "i2c_support.h"
#include <stdio.h>

#define TMP_ADDRESS 0x48

void temperature_task(void *parameter)
{
    i2c_writeWideRegisterSensors(TMP_ADDRESS, 0x01, 0x6100); // Shutdown & Resolution bits set

    for (;;)
    {
        i2c_writeWideRegisterSensors(TMP_ADDRESS, 0x01, 0xE000); // OS and Resolution bits for one-shot conversion
        vTaskDelay(pdMS_TO_TICKS(26));                           // one converstion takes 26ms
        uint16_t raw_data = i2c_readWideRegisterSensors(TMP_ADDRESS, 0x00);

        raw_data = (raw_data >> 8) | (raw_data << 8);

        if (raw_data & 0x8000) // check for a negative value
        {
            raw_data = -((~raw_data) + 1);
        }
        float temperature = (float)raw_data / 256.0;

        printf("TMP102 : Temperature = %f°C\n", temperature);
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void init_temperature(void)
{
    xTaskCreate(temperature_task, "temperature", 1000, NULL, 10, NULL);
}
