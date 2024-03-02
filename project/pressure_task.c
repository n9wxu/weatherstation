#include "FreeRTOS.h"
#include "task.h"

#include "i2c_support.h"
#include <math.h>
#include "leds.h"
#include <stdio.h>

#include "reporting_task.h"

/** monitor the temperature and pressure from a BMP388 every minute or so */

#define BMP_ADDRESS 0x77

/** BMP388 settings for a weather station (page 17 of the datasheet)
 * Mode : Forced
 * Over-Sampling : Ultra Low Power
 * osrs_p : *1
 * osrs_t : *1
 * IIR filter : OFF
 * ODR [Hz] : 1/60
 * RMS Noise [cm] : 55
 */

/** BMP388 registers */
#define TRIM 0x31
#define CMD 0x7E
#define CONFIG 0x1F
#define ODR 0x1D
#define OSR 0x1C
#define PWR_CTRL 0x1B
#define IF_CONF 0x1A
#define INT_CTRL 0x19
#define FIFO_CONFIG_2 0x18
#define FIFO_CONFIG_1 0x17
#define FIFO_WTM_1 0x16
#define FIFO_WTM_0 0x15
#define FIFO_DATA 0x14
#define FIFO_LENGTH_1 0x13
#define FIFO_LENGTH_0 0x12
#define INT_STATUS 0x11
#define EVENT 0x10
#define SENSORTIME2 0x0E
#define SENSORTIME1 0x0D
#define SENSORTIME0 0x0C
#define DATA_5 0x09
#define DATA_4 0x08
#define DATA_3 0x07
#define DATA_2 0x06
#define DATA_1 0x05
#define DATA_0 0x04
#define STATUS 0x03
#define ERR_REG 0x02
#define CHIP_ID 0x00

static float temperature_compensation(int32_t temperature_raw);
static float pressure_compensation(int32_t pressure_raw);

struct FloatParams
{ // The BMP388 float point compensation trim parameters
    float param_T1;
    float param_T2;
    float param_T3;
    float param_P1;
    float param_P2;
    float param_P3;
    float param_P4;
    float param_P5;
    float param_P6;
    float param_P7;
    float param_P8;
    float param_P9;
    float param_P10;
    float param_P11;
    float temperature; // linearized temperature for the pressure sensor
} floatParams;

static void getTrimParameters()
{
    struct parameter_t
    { // The BMP388 compensation trim parameters (coefficients)
        uint16_t param_T1;
        uint16_t param_T2;
        int8_t param_T3;
        int16_t param_P1;
        int16_t param_P2;
        int8_t param_P3;
        int8_t param_P4;
        uint16_t param_P5;
        uint16_t param_P6;
        int8_t param_P7;
        int8_t param_P8;
        int16_t param_P9;
        int8_t param_P10;
        int8_t param_P11;
    } __attribute__((packed)) params;

    i2c_readRegisterBlockSensors(BMP_ADDRESS, TRIM, (uint8_t *)&params, sizeof(params));

    floatParams.param_T1 = (float)params.param_T1 / powf(2.0f, -8.0f); // Calculate the floating point trim parameters
    floatParams.param_T2 = (float)params.param_T2 / powf(2.0f, 30.0f);
    floatParams.param_T3 = (float)params.param_T3 / powf(2.0f, 48.0f);
    floatParams.param_P1 = ((float)params.param_P1 - powf(2.0f, 14.0f)) / powf(2.0f, 20.0f);
    floatParams.param_P2 = ((float)params.param_P2 - powf(2.0f, 14.0f)) / powf(2.0f, 29.0f);
    floatParams.param_P3 = (float)params.param_P3 / powf(2.0f, 32.0f);
    floatParams.param_P4 = (float)params.param_P4 / powf(2.0f, 37.0f);
    floatParams.param_P5 = (float)params.param_P5 / powf(2.0f, -3.0f);
    floatParams.param_P6 = (float)params.param_P6 / powf(2.0f, 6.0f);
    floatParams.param_P7 = (float)params.param_P7 / powf(2.0f, 8.0f);
    floatParams.param_P8 = (float)params.param_P8 / powf(2.0f, 15.0f);
    floatParams.param_P9 = (float)params.param_P9 / powf(2.0f, 48.0f);
    floatParams.param_P10 = (float)params.param_P10 / powf(2.0f, 48.0f);
    floatParams.param_P11 = (float)params.param_P11 / powf(2.0f, 65.0f);
}

static void pressure_task(void *parameter)
{
    printf("BMP388 : ");
    vPortYield();

    i2c_writeRegisterSensors(BMP_ADDRESS, CMD, 0xB6);

    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t event = i2c_readRegisterSensors(BMP_ADDRESS, EVENT);
    if (event & 0x01)
    {
        printf("Reset\n");
        vPortYield();
    }
    else
    {
        printf("Reset Failed\n");
        vPortYield();
        assert(false);
    }

    printf("BMP388 : Fetching trim parameters : ");
    vPortYield();
    getTrimParameters();
    printf("Finished\n");
    vPortYield();
    // Configuring the OSR for Temperature & Pressure
    // No oversampling
    i2c_writeRegisterSensors(BMP_ADDRESS, OSR, 0x00);

    for (;;)
    {

        uint8_t r;

        putBMPLED(true);
        // Start a Power and Temperature Forced cycle
        i2c_writeRegisterSensors(BMP_ADDRESS, PWR_CTRL, 0b00010011);

        // poll to determine if the data is ready.
        // wait for conversion to finish
        do
        {
            vTaskDelay(pdMS_TO_TICKS(2)); // poll slowly as the measurement takes a few ms and we are not in a hurry.
            r = i2c_readRegisterSensors(BMP_ADDRESS, INT_STATUS);
        } while ((r & 0x08) != 0x08);

        // read out the data in a burst
        uint8_t data[6];
        i2c_readRegisterBlockSensors(BMP_ADDRESS, DATA_0, data, sizeof(data));
        uint32_t pressure = data[2] << 16 | data[1] << 8 | data[0];
        uint32_t temperature = data[5] << 16 | data[4] << 8 | data[3];

        reportBMPData(temperature_compensation(temperature), pressure_compensation(pressure));

        putBMPLED(false);

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void init_pressure(void)
{
    xTaskCreate(pressure_task, "pressure", 1000, NULL, 10, NULL);
}

static float temperature_compensation(int32_t temperature_raw)
{
    // use the datasheet compensation formula

    float temp = (float)temperature_raw;
    float partial_data1;
    float partial_data2;

    partial_data1 = temp - floatParams.param_T1;
    partial_data2 = partial_data1 * floatParams.param_T2;
    floatParams.temperature = partial_data1 * partial_data1 * floatParams.param_T3;
    floatParams.temperature += partial_data2;
    return floatParams.temperature;
}

static float pressure_compensation(int32_t pressure_raw)
{
    float pressure = (float)pressure_raw;
    float compensated_pressure;
    float partial_data1;
    float partial_data2;
    float partial_data3;
    float partial_data4;
    float partial_out1;
    float partial_out2;

    partial_data1 = floatParams.param_P6 * floatParams.temperature;
    partial_data2 = floatParams.param_P7 * floatParams.temperature * floatParams.temperature;
    partial_data3 = floatParams.param_P8 * floatParams.temperature * floatParams.temperature * floatParams.temperature;
    partial_out1 = floatParams.param_P5 + partial_data1 + partial_data2 + partial_data3;

    partial_data1 = floatParams.param_P2 * floatParams.temperature;
    partial_data2 = floatParams.param_P3 * floatParams.temperature * floatParams.temperature;
    partial_data3 = floatParams.param_P4 * floatParams.temperature * floatParams.temperature * floatParams.temperature;
    partial_out2 = pressure * (floatParams.param_P1 + partial_data1 + partial_data2 + partial_data3);

    partial_data1 = pressure * pressure;
    partial_data2 = floatParams.param_P9 + floatParams.param_P10 * floatParams.temperature;
    partial_data3 = partial_data1 * partial_data2;
    partial_data4 = partial_data3 + pressure * pressure * pressure * floatParams.param_P11;

    compensated_pressure = partial_out1 + partial_out2 + partial_data4;

    return compensated_pressure;
}