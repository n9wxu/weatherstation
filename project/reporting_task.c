#include "FreeRTOS.h"
#include <stdio.h>
#include "semphr.h"

#define REPORTING_PRIORITY 5

struct data_report_s
{
    float bmp_temperature;
    float bmp_pressure;
    float tmp_temperature;
    float latitude;
    float longtitude;
    float altitude;
    int wind_counts;
    int wind_direction;
    int rain_counts;
};

static struct data_report_s theData;

SemaphoreHandle_t dataMutex;

void reporting_task(void *parameter)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        struct data_report_s dataCopy = theData;
        xSemaphoreGive(dataMutex);
        int now = xTaskGetTickCount() / portTICK_RATE_MS;
        // format and send the data copy
        char buffer[250];
        snprintf(buffer, sizeof(buffer),
                 "{\"BMP\":{\"temperature\":%3.2f,\"pressure\":%3.2f},"
                 "\"TMP\":{\"temperature\":%3.2f},"
                 "\"GPS\":{\"latitude\":%3.5f,\"longitude\":%3.5f, \"altitude\":%.1f},"
                 "\"WND\":{\"counts\":%d,\"direction\":%d},"
                 "\"RAN\":{\"counts\":%d},\"time\":%d}",
                 dataCopy.bmp_temperature, dataCopy.bmp_pressure,
                 dataCopy.tmp_temperature,
                 dataCopy.latitude, dataCopy.longtitude, dataCopy.altitude,
                 dataCopy.wind_counts, dataCopy.wind_direction,
                 dataCopy.rain_counts, now);

        printf("%s\n", buffer);
    }
}

void init_reporting(void)
{
    dataMutex = xSemaphoreCreateMutex();
    xTaskCreate(reporting_task, "reporting", 1000, NULL, REPORTING_PRIORITY, NULL);
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
void reportWINDData(int counts, int direction_degrees)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.wind_counts = counts;
    theData.wind_direction = direction_degrees;
    xSemaphoreGive(dataMutex);
}
void reportRAINData(int tips)
{
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    theData.rain_counts = tips;
    xSemaphoreGive(dataMutex);
}
