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

#define EL_RST_PIN CLICK_RST_PIN     // used by NORA & SARA & C6-Mini for Chip Reset (C6 uses chip reset for el reset)
#define EL_RSN_PIN CLICK_AN_PIN      // used by NORA for EL reset
#define EL_SARA_PWR_PIN CLICK_AN_PIN // used by SARA for EL Power

#define EL_RX_BUFFER_SIZE 10240 // 10KB receive buffer to cover the biggest message

static void el_waitForEvent()
{
    puts("EL: Waiting for Event");
    while (gpio_get(EL_EVENT_PIN) == 0)
    {
        putchar('.');
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    puts("EL: Event found");
}

static void el_waitForAT()
{
    puts("EL: Waiting for AT");
    while (EL_OK != expresslinkSendCommand("AT", NULL, 0))
    {
        putchar('.');
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    puts("EL: AT found\n");
}

static void el_power()
{
    puts("EL: powering");
    gpio_put(EL_SARA_PWR_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_put(EL_SARA_PWR_PIN, false);
    vTaskDelay(pdMS_TO_TICKS(100));
}

/* HW specific UART functions. */
static void el_reset()
{
    puts("EL: resetting");

    gpio_set_dir(EL_RSN_PIN, true);
    gpio_put(EL_RSN_PIN, true);

    gpio_put(EL_RST_PIN, true);
    gpio_set_dir(EL_RST_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_put(EL_RST_PIN, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_put(EL_RST_PIN, true);
}

static void el_flush()
{
    puts("el_flush: flushing");
    while (uart_is_readable_within_us(EL_UART, 2000000)) // wait 2 seconds for any characters
    {
        uart_getc(EL_UART);
    }
}

// send the command .. append \r\n
static void el_write(const char *string)
{
    const char *c = string;
    printf("el_write: %s\n", string);
    while (*c)
        uart_putc_raw(EL_UART, *c++);
    uart_putc_raw(EL_UART, '\r');
    uart_putc_raw(EL_UART, '\n');
}

static char el_rx_buffer[EL_RX_BUFFER_SIZE];

static TaskHandle_t readTask;
static void el_on_uart_rx()
{ 
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    static int buffer_position = 0;

    while (uart_is_readable(EL_UART))
    {
        char ch = uart_getc(EL_UART);
        switch (ch)
        {
        case '\r': // discard this character
            break;
        case '\n': // Task notification
            el_rx_buffer[buffer_position] = 0;
            xTaskNotifyFromISR(readTask, buffer_position, eSetValueWithOverwrite, &higherPriorityTaskWoken);
            uart_set_irq_enables(EL_UART, false, false); // turn off interrupts
            buffer_position = 0;
            break;
        default:
            el_rx_buffer[buffer_position] = ch;
            buffer_position++;
            if (buffer_position > sizeof(el_rx_buffer) - 1) // truncate if a line is too long
            {
                buffer_position = sizeof(el_rx_buffer) - 1;
            }
            break;
        }
    }
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

#define EL_COMMAND_TIMEOUT (30 * 1000)
// read one line of data filling the buffer
// ensure.  ignore leading '\r's and '\n's.
// return on the first nonleading '\r' or '\n'
// return value is the number of received characters
// if the bufferlen is too small, receive all the data the buffer
// will hold and then continue receiving until the end of the line
static int el_read(char *const buffer, size_t bufferLen)
{
    uint32_t i = 0;
    bool receivingLine = true;
    char *dst = buffer;

    memset(el_rx_buffer, 0, sizeof(el_rx_buffer));
    readTask = xTaskGetCurrentTaskHandle();
    uart_set_irq_enables(EL_UART, true, false);
    TickType_t startTime = xTaskGetTickCount();

    if (pdTRUE == xTaskNotifyWaitIndexed(0, 0x00, -1, &i, pdMS_TO_TICKS(EL_COMMAND_TIMEOUT)))
    {
        memcpy(buffer, el_rx_buffer, i);
        printf("el_read: %.*s\n", bufferLen, buffer);
    }
    else
    {
        puts("el_read timeout");
    }
    return i;
}

// Get the response code from the begining of an ExpressLink response
static response_codes_t el_checkResponse(const char *response)
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
    char buffer[200];
    el_write(command);
    int l = el_read(buffer, sizeof(buffer));
    if (l)
    {
        response_codes_t value = el_checkResponse(buffer);
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

static bool el_setup()
{
    char thingName[50];
    char topicBuffer[75];

    expresslinkGetThingName(thingName, sizeof(thingName));
    snprintf(topicBuffer, sizeof(topicBuffer), "AT+CONF Topic1=raw_weather_data/%s", thingName);
    if (EL_OK != expresslinkSendCommand(topicBuffer, NULL, 0))
    {
        puts("Topic 1 set failure");
        return false;
    }
    snprintf(topicBuffer, sizeof(topicBuffer), "AT+CONF Topic2=scaled_weather_data/%s", thingName);
    if (EL_OK != expresslinkSendCommand(topicBuffer, NULL, 0))
    {
        puts("Topic 2 set failure");
        return false;
    }
    snprintf(topicBuffer, sizeof(topicBuffer), "AT+CONF Topic3=scaled_weather_data/all");
    if (EL_OK != expresslinkSendCommand(topicBuffer, NULL, 0))
    {
        puts("Topic 3 set failure");
        return false;
    }
    return true;
}

/*********************************************************************************
 * Public Interfaces
 *********************************************************************************/
bool expresslinkIsConnected()
{
    char responseBuffer[50];
    bool returnValue = false;
    if (expresslinkSendCommand("AT+CONNECT?", responseBuffer, sizeof(responseBuffer)) == EL_OK)
    {
        if (strnstr(responseBuffer, "OK 1", 4) != NULL)
        {
            returnValue = true;
        }
    }
    return returnValue;
}

void expresslinkConnect()
{
    char responseBuffer[50];
    bool finished = false;
    int retryCount = 0;

    puts("Connecting");
    printf("Connecting ExpressLink:");
    do
    {
        if ((++retryCount) > 4)
        {
            puts("ExpressLink Reset");
            el_reset();
            el_waitForEvent();
            el_waitForAT();
        }
        if (el_setup())
        {
            if (expresslinkSendCommand("AT+CONNECT", responseBuffer, sizeof(responseBuffer)) == EL_OK)
            {
                if (strnstr(responseBuffer, "OK 1", 4) != NULL)
                {
                    finished = true;
                    puts("Connection Complete");
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
                el_waitForAT();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    } while (!finished);
    puts("Expresslink Connected");
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
    uart_init(EL_UART, EL_BAUD);
    gpio_set_function(CLICK_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(CLICK_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(EL_UART, false, false);
    uart_set_format(EL_UART, EL_DATA_BITS, EL_STOP_BITS, EL_PARITY);
    uart_set_fifo_enabled(EL_UART, true);
    uart_set_irq_enables(EL_UART, false, false);
    const int UART_IRQ = EL_UART == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, el_on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    gpio_init(EL_RST_PIN);
    gpio_init(EL_SARA_PWR_PIN);
    gpio_init(EL_WAKE_PIN);
    gpio_set_dir(EL_WAKE_PIN, true);
    gpio_put(EL_WAKE_PIN, true);
    gpio_init(EL_EVENT_PIN);
    gpio_set_dir(EL_EVENT_PIN, false);

    el_power(); // Cellular ExpressLink needs to be powered on.
    el_reset(); // Reset any ExpressLink
    el_waitForEvent();
    el_flush();     // flush any characters in the UART
    el_waitForAT(); // Send AT and expect OK.
}