#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include "reporting_task.h"

#include "gps.h"
#include "leds.h"

#define GPS_TX_PIN 5 // The GPS is sending on this pin so it must connect to RX
#define GPS_RX_PIN 4 // The GPS is receiving on this pin so it must connect to TX

#define GPS_UART uart1
#define GPS_BAUD 115200
#define GPS_DATA_BITS 8
#define GPS_STOP_BITS 1
#define GPS_PARITY UART_PARITY_NONE

static QueueHandle_t gpsQueue;
typedef char nmea_buffer_t[85];

struct gps_date_t
{
    bool goodTime;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

SemaphoreHandle_t dateSemaphore;
struct gps_date_t gpsDate = {false};

void on_uart_rx()
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    static nmea_buffer_t nmea_buffer = {0}; // longest NMEA string is 82 bytes
    static int buffer_position = 0;

    while (uart_is_readable(GPS_UART))
    {
        char ch = uart_getc(GPS_UART);
        switch (ch)
        {
        case '\r': // discard this character
            break;
        case '\n': // post the message on the GPS queue
            xQueueSendFromISR(gpsQueue, &nmea_buffer, &higherPriorityTaskWoken);
            buffer_position = 0;
            memset(nmea_buffer, 0, sizeof(nmea_buffer));
            break;
        default:
            nmea_buffer[buffer_position] = ch;
            buffer_position++;
            if (buffer_position > sizeof(nmea_buffer) - 1)
            {
                buffer_position = sizeof(nmea_buffer) - 1;
            }
            break;
        }
    }
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void print_tpv_value(const char *name, const char *format, const int32_t value, const int32_t scale_factor)
{
    printf("%s: ", name);
    if (GPS_INVALID_VALUE != value)
    {
        printf(format, (double)value / scale_factor);
    }
    else
    {
        puts("INVALID");
    }
}

static void gps_task(void *parameter)
{
    struct gps_tpv tpv;
    gps_init_tpv(&tpv);

    for (;;)
    {
        nmea_buffer_t nmea_message = {0};
        if (xQueueReceive(gpsQueue, &nmea_message, pdMS_TO_TICKS(2000)) == pdTRUE)
        {
            bool report = false;
            strncat(nmea_message, "\r\n", 3);
            gps_init_tpv(&tpv);
            int gps_error = gps_decode(&tpv, nmea_message);
            if (GPS_OK == gps_error)
            {
                switch (tpv.mode)
                {
                case GPS_MODE_3D_FIX:
                    putGPSLED(true);
                    report = true;
                    if (strlen(tpv.time) > 0)
                    {
                        float s;
                        struct gps_date_t aDate;
                        sscanf(tpv.time, "%d-%d-%dT%d:%d:%fZ", &aDate.year,
                               &aDate.month, &aDate.day, &aDate.hour, &aDate.minute, &s);
                        aDate.second = (int)s;
                        aDate.goodTime = true;
                        if (pdTRUE == xSemaphoreTake(dateSemaphore, pdMS_TO_TICKS(10)))
                        {
                            gpsDate = aDate;
                            xSemaphoreGive(dateSemaphore);
                            /* add some code to set the RTC from the GPS every so often */
                            printf("GPS Time: %s\n", tpv.time);
                        }
                    }
                    break;
                case GPS_MODE_2D_FIX:
                    putGPSLED(true);
                    report = true;
                    break;
                case GPS_MODE_NO_FIX:
                    putGPSLED(false);
                    break;
                case GPS_MODE_UNKNOWN:
                    putGPSLED(false);
                    break;
                default:
                    break;
                }
                if (report)
                {
                    reportGPSData(tpv.latitude, tpv.longitude, tpv.altitude);
                }
            }
        }
        else
        {
            puts("No GPS data for 2 seconds.");
        }
    }
}

void init_gps(void)
{
    dateSemaphore = xSemaphoreCreateBinary();
    gpsQueue = xQueueCreate(10, sizeof(nmea_buffer_t));
    xTaskCreate(gps_task, "GPS", 1000, NULL, 10, NULL);

    uart_init(GPS_UART, GPS_BAUD);
    gpio_set_function(GPS_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(GPS_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(GPS_UART, false, false);
    uart_set_format(GPS_UART, GPS_DATA_BITS, GPS_STOP_BITS, GPS_PARITY);
    uart_set_fifo_enabled(GPS_UART, true);
    const int UART_IRQ = GPS_UART == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    uart_set_irq_enables(GPS_UART, true, false);
}

static int day_of_week(int y, int m, int d)
{
    return (d += m < 3 ? y-- : y - 2, 23 * m / 9 + d + 4 + y / 4 - y / 100 + y / 400) % 7;
}

BaseType_t gps_setTime()
{
    BaseType_t returnValue = pdFAIL;
    datetime_t rtc_time;

    if (rtc_get_datetime(&rtc_time) == false)
    {
        struct gps_date_t theDate = {false};
        if (pdTRUE == xSemaphoreTake(dateSemaphore, pdMS_TO_TICKS(10)))
        {
            theDate = gpsDate;
            xSemaphoreGive(dateSemaphore);
            if (theDate.goodTime)
            {
                rtc_time.year = theDate.year;
                rtc_time.month = theDate.month;
                rtc_time.day = theDate.day;
                rtc_time.dotw = day_of_week(theDate.year, theDate.month, theDate.day);
                rtc_time.hour = theDate.hour;
                rtc_time.min = theDate.minute;
                rtc_time.sec = theDate.second;
                rtc_init();
                rtc_set_datetime(&rtc_time);

                returnValue = pdPASS;
            }
        }
    }
    return returnValue;
}