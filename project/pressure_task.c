#include "FreeRTOS.h"
#include "task.h"

#include "hardware/i2c.h"

#include <stdio.h>

/** monitor the temperature and pressure from a BMP388 every minute or so */

#define BMP_ADDRESS 0x77

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr)
{
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

static void pressure_task(void *parameter)
{
    for (;;)
    {
        printf("\nI2C Bus Scan\n");
        printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

        for (int addr = 0; addr < (1 << 7); ++addr)
        {
            if (addr % 16 == 0)
            {
                printf("%02x ", addr);
            }

            // Perform a 1-byte dummy read from the probe address. If a slave
            // acknowledges this address, the function returns the number of bytes
            // transferred. If the address byte is ignored, the function returns
            // -1.

            // Skip over any reserved addresses.
            int ret;
            uint8_t rxdata;
            if (reserved_addr(addr))
                ret = PICO_ERROR_GENERIC;
            else
                ret = i2c_read_blocking(i2c0, addr, &rxdata, 1, false);

            printf(ret < 0 ? "." : "@");
            printf(addr % 16 == 15 ? "\n" : "  ");
        }
        printf("Done.\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void readRegister(char reg, char *buffer, size_t bufferSize)
{
    *buffer = 0xA5;
    i2c_write_blocking(i2c0, BMP_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c0, BMP_ADDRESS, buffer, bufferSize, false);
}

void init_pressure(void)
{
    xTaskCreate(pressure_task, "pressure", 1000, NULL, 10, NULL);
}
