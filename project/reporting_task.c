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

struct volts_report_s
{
    SemaphoreHandle_t dataMutex;
    float volts;
};

struct gps_report_s
{
    SemaphoreHandle_t dataMutex;
    float latitude;
    float longtitude;
    float altitude;
};

struct tmp_report_s
{
    SemaphoreHandle_t dataMutex;
    float tmp_temperature;
};

struct bmp_report_s
{
    SemaphoreHandle_t dataMutex;
    float temperature;
    float pressure;
};

struct wind_report_s
{
    SemaphoreHandle_t dataMutex;
    unsigned int wind_counts;
    int wind_direction;
    float windSpeed_2m;
    float gustSpeed_10m;
    int windDirection_2m;
    int gustDirection_10m;
};

struct rain_report_s
{
    SemaphoreHandle_t dataMutex;
    unsigned int rain_counts;
    float rain_in_hr;
    float rain_in_day;
};

static struct gps_report_s gpsData;
static struct wind_report_s windData;
static struct rain_report_s rainData;
static struct bmp_report_s bmpData;
static struct tmp_report_s tmpData;
static struct volts_report_s voltsData;

void reporting_task(void *parameter)
{
    char thingName[50];

    struct data_report_s
    {
        float rain_in_hr;
        float rain_in_day;
        unsigned int rain_counts;
        unsigned int wind_counts;
        int wind_direction;
        float windSpeed_2m;
        float gustSpeed_10m;
        int windDirection_2m;
        int gustDirection_10m;
        float bmp_temperature;
        float bmp_pressure;
        float latitude;
        float longtitude;
        float altitude;
        float tmp_temperature;
        float volts;
    };

    expresslinkInit();

    expresslinkGetThingName(thingName, sizeof(thingName));

    TickType_t previousWakeTime = xTaskGetTickCount();
    for (;;)
    {
        vTaskDelayUntil(&previousWakeTime, pdMS_TO_TICKS(60000));
        struct data_report_s dataCopy;
        puts("collecting rain data");
        xSemaphoreTake(rainData.dataMutex, pdMS_TO_TICKS(1));
        dataCopy.rain_in_day = rainData.rain_in_day;
        dataCopy.rain_in_hr = rainData.rain_in_hr;
        dataCopy.rain_counts = rainData.rain_counts;
        xSemaphoreGive(rainData.dataMutex);
        puts("Done with rain data");
        puts("Collecting wind data");
        xSemaphoreTake(windData.dataMutex, pdMS_TO_TICKS(1));
        dataCopy.wind_counts = windData.wind_counts;
        dataCopy.wind_direction = windData.wind_direction;
        dataCopy.windDirection_2m = windData.windDirection_2m;
        dataCopy.windSpeed_2m = windData.windSpeed_2m;
        dataCopy.gustDirection_10m = windData.gustDirection_10m;
        dataCopy.gustSpeed_10m = windData.gustSpeed_10m;
        xSemaphoreGive(windData.dataMutex);
        puts("Done with wind data");
        puts("Collecting gps data");
        xSemaphoreTake(gpsData.dataMutex, pdMS_TO_TICKS(1));
        dataCopy.latitude = gpsData.latitude;
        dataCopy.longtitude = gpsData.longtitude;
        dataCopy.altitude = gpsData.altitude;
        xSemaphoreGive(gpsData.dataMutex);
        puts("Done with gps data");
        puts("Collecting bmp data");
        xSemaphoreTake(bmpData.dataMutex, pdMS_TO_TICKS(1));
        dataCopy.bmp_pressure = bmpData.pressure;
        dataCopy.bmp_temperature = bmpData.temperature;
        xSemaphoreGive(bmpData.dataMutex);
        puts("Done with bmp data");
        puts("Collecting tmp data");
        xSemaphoreTake(tmpData.dataMutex, pdMS_TO_TICKS(1));
        dataCopy.tmp_temperature = tmpData.tmp_temperature;
        xSemaphoreGive(tmpData.dataMutex);
        puts("Done with tmp data");
        puts("Collecting volts data");
        xSemaphoreTake(voltsData.dataMutex, pdMS_TO_TICKS(1));
        dataCopy.volts = voltsData.volts;
        xSemaphoreGive(voltsData.dataMutex);
        puts("Done with volts data");
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
                 "\"GPS\":{\"latitude\":%.g,\"longitude\":%.5g, \"altitude\":%.1g},"
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
        // disconnecting and reconnecting costs 10KB of data which is expensive on a Cellular connection
        //        expresslinkDisconnect();
        putRPTLED(false);
    }
}

void init_reporting(void)
{
    bmpData.dataMutex = xSemaphoreCreateMutex();
    tmpData.dataMutex = xSemaphoreCreateMutex();
    gpsData.dataMutex = xSemaphoreCreateMutex();
    windData.dataMutex = xSemaphoreCreateMutex();
    rainData.dataMutex = xSemaphoreCreateMutex();
    voltsData.dataMutex = xSemaphoreCreateMutex();
    xTaskCreate(reporting_task, "reporting", 8196, NULL, REPORTING_PRIORITY, NULL);
}

void reportBMPData(float temperature, float pressure)
{
    xSemaphoreTake(bmpData.dataMutex, pdMS_TO_TICKS(1));
    bmpData.pressure = pressure;
    bmpData.temperature = temperature;
    xSemaphoreGive(bmpData.dataMutex);
}
void reportTMPData(float temperature)
{
    xSemaphoreTake(tmpData.dataMutex, pdMS_TO_TICKS(1));
    tmpData.tmp_temperature = temperature;
    xSemaphoreGive(tmpData.dataMutex);
}
void reportGPSData(float lat, float lng, float altitude)
{
    xSemaphoreTake(gpsData.dataMutex, pdMS_TO_TICKS(1));
    gpsData.latitude = lat;
    gpsData.longtitude = lng;
    gpsData.altitude = altitude;
    xSemaphoreGive(gpsData.dataMutex);
}
void reportWINDData(unsigned int counts, int direction_degrees)
{
    xSemaphoreTake(windData.dataMutex, pdMS_TO_TICKS(1));
    windData.wind_counts = counts;
    windData.wind_direction = direction_degrees;
    xSemaphoreGive(windData.dataMutex);
}
void reportRAINData(unsigned int tips)
{
    xSemaphoreTake(rainData.dataMutex, pdMS_TO_TICKS(1));
    rainData.rain_counts = tips;
    xSemaphoreGive(rainData.dataMutex);
}

void reportRainScaledData(float rain_hr, float rain_day)
{
    xSemaphoreTake(rainData.dataMutex, pdMS_TO_TICKS(1));
    rainData.rain_in_hr = rain_hr;
    rainData.rain_in_day = rain_day;
    xSemaphoreGive(rainData.dataMutex);
}

void reportWINDScaledData(float avgSpeed_2m, int avgDirection_2m, float gustSpeed_10m, int gustDirection_10m)
{
    xSemaphoreTake(windData.dataMutex, pdMS_TO_TICKS(1));
    windData.windSpeed_2m = avgSpeed_2m;
    windData.windDirection_2m = avgDirection_2m;
    windData.gustSpeed_10m = gustSpeed_10m;
    windData.gustDirection_10m = gustDirection_10m;
    xSemaphoreGive(windData.dataMutex);
}

void reportBatteryVoltage(float volts)
{
    xSemaphoreTake(voltsData.dataMutex, pdMS_TO_TICKS(1));
    voltsData.volts = volts;
    xSemaphoreGive(voltsData.dataMutex);
}