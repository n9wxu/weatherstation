#include "FreeRTOS.h"
#include "task.h"

#include "hardware/i2c.h"

#include <stdio.h>

/** monitor the temperature and pressure from a BMP388 every minute or so */

#define BMP_ADDRESS 0x77

static void pressure_task(void *parameter)
{
}

void readRegister(char reg, char *buffer, size_t bufferSize)
{
    *buffer = 0xA5;
    i2c_write_blocking(i2c0, BMP_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c0, BMP_ADDRESS, buffer, bufferSize, false);
}

void init_pressure(void)
{
    uint8_t buf;
    uint8_t reg = 0;
    readRegister(reg, &buf, sizeof(buf));
    printf("BMP ID : 0x%02x\n", buf);
}
