#ifndef _I2C_SUPPORT_
#define _I2C_SUPPORT_

#include <stdint.h>
#include <stdlib.h>

void i2c_sensorInit();

uint8_t i2c_readRegisterSensors(uint8_t address, uint8_t reg);
void i2c_writeRegisterSensors(uint8_t address, uint8_t reg, uint8_t value);

void i2c_readRegisterBlockSensors(uint8_t address, uint8_t reg, uint8_t *buffer, size_t bufferLen);

uint16_t i2c_readWideRegisterSensors(uint8_t address, uint8_t reg);
void i2c_writeWideRegisterSensors(uint8_t address, uint8_t reg, uint16_t value);

#endif // _I2C_SUPPORT_