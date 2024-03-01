#include "FreeRTOS.h"
#include "semphr.h"

#include "i2c_support.h"
#include <stdbool.h>

#include "hardware/i2c.h"
#include "hardware/gpio.h"

#include "pinmap.h"

SemaphoreHandle_t i2c_semaphore;

#define IC2_SELECTION i2c0
const int I2C_BAUDRATE = 400 * 1000; // 400khz baudrate

void i2c_sensorInit()
{
    i2c_semaphore = xSemaphoreCreateMutex();

    i2c_init(IC2_SELECTION, I2C_BAUDRATE);
    gpio_set_function(I2C_SCK_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCK_PIN);
    gpio_pull_up(I2C_SDA_PIN);
}

uint8_t i2c_readRegisterSensors(uint8_t address, uint8_t reg)
{
    uint8_t v;
    BaseType_t result = xSemaphoreTake(i2c_semaphore, portMAX_DELAY);
    assert(result == pdTRUE);
    i2c_write_blocking(IC2_SELECTION, address, &reg, 1, true);
    i2c_read_blocking(IC2_SELECTION, address, &v, 1, false);
    xSemaphoreGive(i2c_semaphore);
    return v;
}

void i2c_writeRegisterSensors(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = {reg, value};
    BaseType_t result = xSemaphoreTake(i2c_semaphore, portMAX_DELAY);
    assert(result == pdTRUE);
    i2c_write_blocking(IC2_SELECTION, address, buffer, 2, false);
    xSemaphoreGive(i2c_semaphore);
}

void i2c_readRegisterBlockSensors(uint8_t address, uint8_t reg, uint8_t *buffer, size_t bufferLen)
{
    BaseType_t result = xSemaphoreTake(i2c_semaphore, portMAX_DELAY);
    assert(result == pdTRUE);
    i2c_write_blocking(IC2_SELECTION, address, &reg, 1, true);
    i2c_read_blocking(IC2_SELECTION, address, buffer, bufferLen, false);
    xSemaphoreGive(i2c_semaphore);
}

uint16_t i2c_readWideRegisterSensors(uint8_t address, uint8_t reg)
{
    uint16_t v;
    BaseType_t result = xSemaphoreTake(i2c_semaphore, portMAX_DELAY);
    assert(result == pdTRUE);
    i2c_write_blocking(IC2_SELECTION, address, &reg, 1, true);
    i2c_read_blocking(IC2_SELECTION, address, (uint8_t *)&v, 2, false);
    xSemaphoreGive(i2c_semaphore);
    return v;
}

void i2c_writeWideRegisterSensors(uint8_t address, uint8_t reg, uint16_t value)
{
    uint8_t buffer[3] = {reg, value >> 8, value};
    BaseType_t result = xSemaphoreTake(i2c_semaphore, portMAX_DELAY);
    assert(result == pdTRUE);
    i2c_write_blocking(IC2_SELECTION, address, buffer, 3, false);
    xSemaphoreGive(i2c_semaphore);
}