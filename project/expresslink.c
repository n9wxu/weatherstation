#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pinmap.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#include "expresslink.h"

#define EL_UART uart0
#define EL_BAUD 115200
#define EL_DATA_BITS 8
#define EL_STOP_BITS 1
#define EL_PARITY UART_PARITY_NONE

#define EL_WAKE_PIN CLICK_PWM_PIN
#define EL_EVENT_PIN CLICK_INT_PIN
#define EL_RESET_PIN CLICK_RST_PIN
#define EL_SARA_PWR_PIN CLICK_AN_PIN

static void el_waitForEvent();

/* HW specific UART functions. */
void el_reset()
{
    puts("resetting Expresslink");
    gpio_put(EL_RESET_PIN, true);
    gpio_set_dir(EL_RESET_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_put(EL_RESET_PIN, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_dir(EL_RESET_PIN, false);
}

void el_flush()
{
    puts("el_flush: flushing");
    while (uart_is_readable_within_us(EL_UART, 2000000)) // wait 2 seconds for any characters
    {
        uart_getc(EL_UART);
    }
}

// send the command .. append \r\n
void el_write(const char *string)
{
    const char *c = string;
    printf("el_write: %s\n", string);
    while (*c)
        uart_putc_raw(EL_UART, *c++);
    uart_putc_raw(EL_UART, '\r');
    uart_putc_raw(EL_UART, '\n');
}

// read one line of data filling the buffer
// ensure.  ignore leading '\r's and '\n's.
// return on the first nonleading '\r' or '\n'
// return value is the number of received characters
// if the bufferlen is too small, receive all the data the buffer
// will hold and then continue receiving until the end of the line
int el_read(char *const buffer, size_t bufferLen)
{
    int i = 0;
    bool receivingLine = true;
    char *dst = buffer;

    if (uart_is_readable_within_us(EL_UART, 30000000UL))
    {
        while (receivingLine)
        {
            char c = uart_getc(EL_UART);
            if (c == '\r' || c == '\n')
            {
                if (i != 0)
                {
                    receivingLine = false;
                }
            }
            else
            {
                if (i < (bufferLen - 1))
                {
                    *dst++ = c;
                    *dst = 0;
                    i++;
                }
            }
        }
        printf("el_read: %.*s\n", bufferLen, buffer);
    }
    else
    {
        puts("el_read: no response in 30 seconds");
    }
    return i;
}

// Get the response code from the begining of an ExpressLink response
static response_codes_t checkResponse(const char *response)
{
    response_codes_t value = EL_NORESPONSE;
    char *okPosition = strnstr(response, "OK", 2);
    if (okPosition != NULL)
    {
        value = EL_OK;
    }
    else
    {
        char *errPosition = strnstr(response, "ERR", 3);
        int errCode = atoi(errPosition + 3);
        value = errCode;
    }
    return value;
}

// Send a command and retrieve the response
response_codes_t expresslinkSendCommand(const char *command, char *response, size_t responseLength)
{
    char buffer[500];
    el_write(command);
    int l = el_read(buffer, sizeof(buffer));
    if (l)
    {
        response_codes_t value = checkResponse(buffer);
        if (response && responseLength > 0)
        {
            strncpy(response, buffer, responseLength);
        }
        return value;
    }
    else
    {
        return EL_NORESPONSE;
    }
}

void expresslinkConnect()
{
    char responseBuffer[50];
    bool finished = false;
    int retryCount = 0;

    while (expresslinkSendCommand("AT+CONNECT?", responseBuffer, sizeof(responseBuffer)) == EL_NORESPONSE)
    {
        el_reset();
        el_waitForEvent();
    }

    if (strnstr(responseBuffer, "OK 1", 4) == NULL)
    {
        puts("reconnecting");
        expresslinkSendCommand("AT+EVENT?", NULL, 0);
        printf("Connecting ExpressLink:");
        do
        {
            if ((++retryCount) > 4)
            {
                el_reset();
            }
            if (expresslinkSendCommand("AT+CONNECT", responseBuffer, sizeof(responseBuffer)) == EL_OK)
            {
                if (strnstr(responseBuffer, "OK 1", 4) != NULL)
                {
                    finished = true;
                }
                else
                {
                    printf("EL OK : %s\n", responseBuffer);
                }
            }
            else
            {
                printf("EL Connection Error : %s\n", responseBuffer);
                puts("Reset expresslink");
                el_reset();
                el_waitForEvent();
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        } while (!finished);
        puts("Expresslink Connected");
    }
    else
    {
        puts("still connected");
    }
}

static void el_waitForEvent()
{
    while (gpio_get(EL_EVENT_PIN) == 0)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void el_power()
{
    printf("ExpressLink Initializing Power:");
    gpio_put(EL_SARA_PWR_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_put(EL_SARA_PWR_PIN, false);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void expresslinkGetThingName(char *thingName, size_t thingNameLen)
{
    char buffer[50];
    if (EL_OK == expresslinkSendCommand("AT+CONF? ThingName", buffer, sizeof(buffer)))
    {
        strncpy(thingName, &buffer[3], thingNameLen);
    }
}

void expresslinkPublish(int topic, char *message, size_t messageLength)
{
    char sendBuffer[500];
    snprintf(sendBuffer, sizeof(sendBuffer), "AT+SEND%d %.*s", topic, messageLength, message);
    if (EL_OK != expresslinkSendCommand(sendBuffer, NULL, 0))
    {
        printf("Send Failure %d, %.*s\n", topic, messageLength, message);
    }
}

void expresslinkDisconnect()
{
    if (EL_OK != expresslinkSendCommand("AT+disconnect", NULL, 0))
    {
        printf("Disconnect Failed\n");
    }
}

void expresslinkInit()
{
    char thingName[50];
    char topicBuffer[75];
    uart_init(EL_UART, EL_BAUD);
    gpio_set_function(CLICK_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(CLICK_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(EL_UART, false, false);
    uart_set_format(EL_UART, EL_DATA_BITS, EL_STOP_BITS, EL_PARITY);
    uart_set_fifo_enabled(EL_UART, true);

    gpio_init(EL_RESET_PIN);

    gpio_init(EL_SARA_PWR_PIN);
    gpio_set_dir(EL_SARA_PWR_PIN, true);
    gpio_put(EL_SARA_PWR_PIN, false);

    // CLick ID connects to 1-Wire - ignore
    gpio_init(CLICK_CS_PIN);
    gpio_set_dir(CLICK_CS_PIN, false);

    gpio_init(EL_WAKE_PIN);
    gpio_set_dir(EL_WAKE_PIN, true);
    gpio_put(EL_WAKE_PIN, true);

    gpio_init(EL_EVENT_PIN);
    gpio_set_dir(EL_EVENT_PIN, false);

    vTaskDelay(pdMS_TO_TICKS(500));

    el_power();
    el_reset();
    el_waitForEvent();
    el_flush();

    vTaskDelay(200);
    while (EL_OK != expresslinkSendCommand("AT", NULL, 0))
    {
        vTaskDelay(1);
    }

    expresslinkGetThingName(thingName, sizeof(thingName));
    snprintf(topicBuffer, sizeof(topicBuffer), "AT+CONF Topic1=raw_weather_data/%s", thingName);
    if (EL_OK != expresslinkSendCommand(topicBuffer, NULL, 0))
    {
        printf("Topic 1 set failure");
    }
    snprintf(topicBuffer, sizeof(topicBuffer), "AT+CONF Topic2=scaled_weather_data/%s", thingName);
    if (EL_OK != expresslinkSendCommand(topicBuffer, NULL, 0))
    {
        printf("Topic 2 set failure");
    }
    snprintf(topicBuffer, sizeof(topicBuffer), "AT+CONF Topic3=scaled_weather_data/all");
    if (EL_OK != expresslinkSendCommand(topicBuffer, NULL, 0))
    {
        printf("Topic 3 set failure");
    }
}