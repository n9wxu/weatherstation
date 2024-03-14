#include "FreeRTOS.h"
#include <stdio.h>
#include "semphr.h"

#include "pinmap.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include <string.h>
#include "leds.h"
#include "expresslink.h"

#define REPORTING_PRIORITY 5

struct data_report_s
{
    float bmp_temperature;
    float bmp_pressure;
    float tmp_temperature;
    float latitude;
    float longtitude;
    float altitude;
    unsigned int wind_counts;
    int wind_direction;
    unsigned int rain_counts;

    float windSpeed_2m;
    float gustSpeed_10m;
    int windDirection_2m;
    int gustDirection_10m;

    float rain_in_hr;
    float rain_in_day;

    float volts;
};

static struct data_report_s theData;

SemaphoreHandle_t dataMutex;

void reporting_task(void *parameter)
{
    char thingName[50];
    expresslinkInit();

    expresslinkGetThingName(thingName, sizeof(thingName));

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(60000));
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        struct data_report_s dataCopy = theData;
        xSemaphoreGive(dataMutex);
        unsigned int now = xTaskGetTickCount() / portTICK_RATE_MS;
        // format and send the data copy
        putRPTLED(true);
        // make sure we are connected.
        expresslinkConnect();
        char buffer[400];
        snprintf(buffer, sizeof(buffer),
                 "{\"ID\":\"%s\","
                 "\"VOLTS\":%.2g,"
                 "\"BMP\":{\"temperature\":%.2g,\"pressure\":%.2g},"
                 "\"TMP\":{\"temperature\":%.2g},"
                 "\"GPS\":{\"latitude\":%.gf,\"longitude\":%.5g, \"altitude\":%.1g},"
                 "\"WIND\":{\"counts\":%u,\"direction\":%d},"
                 "\"RAIN\":{\"counts\":%u},\"time_ms\":%u}",
                 thingName, dataCopy.volts,
                 dataCopy.bmp_temperature, dataCopy.bmp_pressure,
                 dataCopy.tmp_temperature,
                 dataCopy.latitude, dataCopy.longtitude, dataCopy.altitude,
                 dataCopy.wind_counts, dataCopy.wind_direction,
                 dataCopy.rain_counts, now);
        expresslinkPublish(1, buffer, sizeof(buffer));

        snprintf(buffer, sizeof(buffer),
                 "{\"ID\":\"%s\","
                 "\"VOLTS\":%.2g,"
                 "\"BMP\":{\"temperature\":%.2g,\"pressure\":%.2g},"
                 "\"TMP\":{\"temperature\":%.2g},"
                 "\"GPS\":{\"latitude\":%.5g,\"longitude\":%0.5g, \"altitude\":%.1g},"
                 "\"WIND\":{\"avg_speed_2min\":%.2g,\"avg_direction_2m\":%d,\"gust_speed_10min\":%.2g,\"gust_direction_10min\":%d},"
                 "\"RAIN\":{\"inches_last_hour\":%.2g,\"inches_last_day\":%.2g},\"time_ms\":%u}",
                 thingName, dataCopy.volts,
                 dataCopy.bmp_temperature, dataCopy.bmp_pressure,
                 dataCopy.tmp_temperature,
                 dataCopy.latitude, dataCopy.longtitude, dataCopy.altitude,
                 dataCopy.windSpeed_2m, dataCopy.windDirection_2m, dataCopy.gustSpeed_10m, dataCopy.gustDirection_10m,
                 dataCopy.rain_in_hr, dataCopy.rain_in_day, now);

        expresslinkPublish(2, buffer, sizeof(buffer));
        expresslinkPublish(3, buffer, sizeof(buffer));
        // disconnecting and reconnecting costs 10KB of data.
        //        expresslinkDisconnect();
        putRPTLED(false);
    }
}

void init_reporting(void)
{
    dataMutex = xSemaphoreCreateMutex();
    xTaskCreate(reporting_task, "reporting", 8196, NULL, REPORTING_PRIORITY, NULL);
}

void reportBMPData(float temperature, float pressure)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.bmp_pressure = pressure;
    theData.bmp_temperature = temperature;
    xSemaphoreGive(dataMutex);
}
void reportTMPData(float temperature)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.tmp_temperature = temperature;
    xSemaphoreGive(dataMutex);
}
void reportGPSData(float lat, float lng, float altitude)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.latitude = lat;
    theData.longtitude = lng;
    theData.altitude = altitude;
    xSemaphoreGive(dataMutex);
}
void reportWINDData(unsigned int counts, int direction_degrees)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.wind_counts = counts;
    theData.wind_direction = direction_degrees;
    xSemaphoreGive(dataMutex);
}
void reportRAINData(unsigned int tips)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.rain_counts = tips;
    xSemaphoreGive(dataMutex);
}

void reportRainScaledData(float rain_hr, float rain_day)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.rain_in_hr = rain_hr;
    theData.rain_in_day = rain_day;
    xSemaphoreGive(dataMutex);
}

void reportWINDScaledData(float avgSpeed_2m, int avgDirection_2m, float gustSpeed_10m, int gustDirection_10m)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.windSpeed_2m = avgSpeed_2m;
    theData.windDirection_2m = avgDirection_2m;
    theData.gustSpeed_10m = gustSpeed_10m;
    theData.gustDirection_10m = gustDirection_10m;
    xSemaphoreGive(dataMutex);
}

void reportBatteryVoltage(float volts)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.volts = volts;
    xSemaphoreGive(dataMutex);
}