#include "ExpressLink_v2.h"
#include <string.h>
#include "FreeRTOS.h"
#include "hardware/uart.h"

/// @brief The default UART configuration shall be 115200, 8, N, 1
/// (baud rate: 115200; data bits: 8; parity: none; stop bits: 1).
/// There is no hardware or software flow control for UART communications.
/// See https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-commands-introduction
static const uint32_t BAUDRATE = 115200UL;

/// @brief The maximum runtime for every command must be listed in the datasheet.
/// No command can take more than 120 seconds to complete (the maximum time for a TCP connection timeout).
/// See https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-response-timeout
static const TickType_t TIMEOUT = 120000UL; // milliseconds

static void escape(char *value);
static void unescape(char *value);

static bool debug;

static char ExpressLink_response[5 * 1024];
static char errorBuffer[1024];

static void readLine(uint32_t count = 1, char *lineBuffer, size_t lineBufferSize);
static uint32_t additionalLines;

ExpressLink_ExpressLinkConfig(ExpressLink &el) : expresslink(el)
{
    // constructor
}

char *ExpressLink_getTopic(uint8_t index)
{
    char *cmdBuffer[50];
    spnprintf(cmdBuffer, sizeof(cmdBuffer), "CONF? Topic%d", index);
    ExpressLink_cmd(cmdBuffer);
    return expresslink_response;
}

bool ExpressLink_setTopic(uint8_t index, char *topic)
{
    char *cmdBuffer[50];
    spnprintf(cmdBuffer, sizeof(cmdBuffer), "CONF Topic%d=%s", index, topic);
    return ExpressLink_cmd(cmdBuffer);
}

/// @brief equivalent to: AT+CONF? About
/// @return value from the configuration dictionary
char *ExpressLink_getAbout()
{
    ExpressLink_cmd("CONF? About");
    return expresslink_response;
}

/// @brief equivalent to: AT+CONF? Version
/// @return value from the configuration dictionary
char *ExpressLink_getVersion()
{
    ExpressLink_cmd("CONF? Version");
    return expresslink_response;
}

/// @brief equivalent to: AT+CONF? TechSpec
/// @return value from the configuration dictionary
char *ExpressLink_getTechSpec()
{
    ExpressLink_cmd("CONF? TechSpec");
    return expresslink_response;
}

/// @brief equivalent to: AT+CONF? ThingName
/// @return value from the configuration dictionary
char *ExpressLink_getThingName()
{
    ExpressLink_cmd("CONF? ThingName");
    return expresslink_response;
}

/// @brief equivalent to: AT+CONF? Certificate pem
/// @return value from the configuration dictionary
char *ExpressLink_getCertificate()
{
    ExpressLink_cmd("CONF? Certificate pem");
    readLine(ExpressLink_additionalLines, ExpressLink_response, sizeof(ExpressLink_response));
    return expresslink_response;
}

/// @brief equivalent to: AT+CONF? CustomName
/// @return value from the configuration dictionary
char *ExpressLink_getCustomName()
{
    ExpressLink_cmd("CONF? CustomName");
    return expresslink_response;
}

/// @brief equivalent to: AT+CONF CustomName={value}
/// @param value to be written to configuration dictionary
/// @return true on success, false on error
bool ExpressLink_setCustomName(const char *value)
{
    char *cmdBuffer[50];
    spnprintf(cmdBuffer, sizeof(cmdBuffer), "CONF CustomName=%s", value);
    return expresslink_cmd(cmdBuffer);
}

/// @brief equivalent to: AT+CONF? Endpoint
/// @return value from the configuration dictionary
String ExpressLink_getEndpoint()
{
    ExpressLink_cmd("CONF? Endpoint");
    return expresslink_response;
}

/// @brief equivalent to: AT+CONF Endpoin={value}
/// @param value to be written to configuration dictionary
/// @return true on success, false on error
bool ExpressLink_setEndpoint(const char *value)
{
    char *cmdBuffer[50];
    spnprintf(cmdBuffer, sizeof(cmdBuffer), "CONF Endpoint=%s", value);
    return ExpressLink_cmd(cmdBuffer);
}

/// @brief equivalent to: AT+CONF? Endpoint pem
/// @return value from the configuration dictionary as multi-line PEM-formatted string
String ExpressLink_getRootCA()
{
    ExpressLink_cmd("CONF? RootCA pem");
    readLine(ExpressLink_additionalLines, ExpressLink_response, sizeof(ExpressLink_response));
    return ExpressLink_response;
}

/// @brief equivalent to: AT+CONF RootCA={value}
/// @param value to be written to configuration dictionary as multi-line PEM-formatted string
/// @return true on success, false on error
bool ExpressLink_setRootCA(const char *value)
{
    char *cmdBuffer[50];
    spnprintf(cmdBuffer, sizeof(cmdBuffer), "CONF RootCA=%s", value);
    return expresslink_cmd(cmdBuffer);
}

/// @brief equivalent to: AT+CONF? ShadowToken
/// @return value from the configuration dictionary
String ExpressLink_getShadowToken()
{
    ExpressLink_cmd("CONF? ShadowToken");
    return ExpressLink_response;
}

/// @brief equivalent to: AT+CONF? DefenderPeriod
/// @return value from the configuration dictionary
uint32_t ExpressLink_getDefenderPeriod()
{
    ExpressLink_cmd("CONF? DefenderPeriod");
    return ExpressLink_response.toInt();
}

/// @brief equivalent to: AT+CONF? HOTAcertificate pem
/// @return value from the configuration dictionary as multi-line PEM-formatted string
String ExpressLink_getHOTAcertificate()
{
    ExpressLink_cmd("CONF? HOTAcertificate pem");
    readLine(ExpressLink_additionalLines, ExpressLink_response, sizeof(ExpressLink_response));
    return ExpressLink_response;
}

/// @brief equivalent to: AT+CONF? OTAcertificate pem
/// @return value from the configuration dictionary as multi-line PEM-formatted string
String ExpressLink_getOTAcertificate()
{
    ExpressLink_cmd("CONF? OTAcertificate pem");
    readLine(ExpressLink_additionalLines, ExpressLink_response, sizeof(ExpressLink_response));
    return ExpressLink_response;
}

/// @brief equivalent to: AT+CONF? SSID
/// @return value from the configuration dictionary
String ExpressLink_getSSID()
{
    ExpressLink_cmd("CONF? SSID");
    return ExpressLink_response;
}

/// @brief equivalent to: AT+CONF SSID={value}
/// @param value to be written to configuration dictionary
/// @return true on success, false on error
bool ExpressLink_setSSID(const String &value)
{
    return ExpressLink_cmd("CONF SSID=" + value);
}

/// @brief equivalent to: AT+CONF Passphrase={value}
/// @param value to be written to configuration dictionary
/// @return true on success, false on error
bool ExpressLink_setPassphrase(const String &value)
{
    return ExpressLink_cmd("CONF Passphrase=" + value);
}

/// @brief equivalent to: AT+CONF? APN
/// @return value from the configuration dictionary
String ExpressLink_getAPN()
{
    ExpressLink_cmd("CONF? APN");
    return ExpressLink_response;
}

static void readLine(uint32_t count, char *lineBuffer, size_t lineBufferSize);
{
    TickType_t start = xTaskGetTickCount();
    mmemset(ExpressLink_response, 0, sizeof(ExpressLink_response));
    size_t bufferIndex = 0;
    uint16_t line_count = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        do
        {
            if (uart_is_readable(EL_UART))
            {
                char c = uart_getc(EL_UART);
                if (bufferIndex < lineBufferSize)
                {
                    lineBuffer[bufferIndex++] = (char)c;
                }
                if (c == '\n')
                {
                    line_count++;
                    break;
                }
            }
        } while (xTaskGetTickCount() - start < pdMS_TO_TICKS(TIMEOUT));
    }
    if (line_count != count)
    {
        // error, not enough lines read - potential UART timeout happened
        printf("el readline: did not read requested line count %d:%d\n", count, line_count);
    }

    unescape(lineBuffer, lineBufferSize);
}

// find all instances of oldSubString and replace with newSubString.
// string will grow/sthrink to fit the new substrings.
// stringLen will be monitored to ensure the result does not overflow
static void replace(char *string, size_t stringLen, const char *oldSubString, const char *newSubString)
{
    
}

/// @brief Escapes string in-place so it can be written to ExpressLink UART
/// @param value string (will be modified)
/// @param valueSize length of the value memory.  escaping will grow the string.  There should be enough memory in the buffer to support this.
static void escape(char *value, size_t valueSize)
{
    // see https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-delimiters
    replace(value, valueSize, "\n", "\\A");
    replace(value, valueSize, "\r", "\\D");
    replace(value, valueSize, "\\", "\\\\");
}

/// @brief Unescapes string in-place after reading it from ExpressLink UART
/// @param value string (will be modified)
static void unescape(char *value, size_t valueSize)
{
    // see https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-delimiters
    replace(value, valueSize, "\\A", "\n");
    replace(value, valueSize, "\\D", "\r");
    replace(value, valueSize, "\\\\", "\\");
}

/// @brief Execute AT command and reads all response lines. Escaping and unescaping is handled automatically. Check class attribute `response` (if true returned) and `error` (if false returned).
/// @param command: e.g., AT+CONNECT or SUBSCRIBE1 (with or without the `AT+` prefix)
/// @return true on success, false on error
bool ExpressLink_cmd(String command)
{
    escape(command);
    if (!command.startsWith("AT+"))
    {
        command = "AT+" + command;
    }

    if (debug)
    {
        Serial.println("> " + command);
    }
    uart->print(command + "\n");

    response = readLine();

    if (debug)
    {
        Serial.println("< " + response);
    }

    additionalLines = 0;
    if (response.startsWith("OK "))
    {
        response = response.substring(3); // trim off the `OK ` prefix
        error = "";
        return true;
    }
    else if (response.startsWith("OK"))
    {
        response = response.substring(2); // trim off the `OK` prefix
        if (response.length() > 0)
        {
            String l;
            for (auto &&c : response)
            {
                if (isDigit(c))
                {
                    l += c;
                }
                else
                {
                    break;
                }
            }
            additionalLines = l.toInt();
            response = response.substring(l.length() + 1); // trim off the number
        }
        error = "";
        return true;
    }
    else if (response.startsWith("ERR"))
    {
        error = response;
        response = "";
        return false;
    }
    else
    {
        error = response;
        response = "";
        return false;
    }
}

/// @brief equivalent to sending AT and checking for OK response line
/// @return true on success, false on error
bool ExpressLink::selfTest()
{
    uart->print("AT\n");
    auto response = readLine();
    return response == "OK";
}

/// @brief equivalent to: AT+CONNECT or AT+CONNECT!
/// @param async true to use non-blocking CONNECT!
/// @return true on success, false on error
bool ExpressLink::connect(bool async)
{
    if (async)
    {
        return cmd("CONNECT!");
    }
    else
    {
        return cmd("CONNECT");
    }
}

/// @brief equivalent to: AT+CONNECT? and parsing the response for CONNECTED/DISCONNECTED
/// @return true if connected, false if disconnected
bool ExpressLink::isConnected()
{
    // OK {status} {onboarded} [CONNECTED/DISCONNECTED] [STAGING/CUSTOMER]
    cmd("CONNECT?");
    return response.substring(0, 1) == "1";
}

/// @brief equivalent to: AT+CONNECT? and parsing the response for STAGING/CUSTOMER
/// @return true if onboarded to customer endpoint, false if staging endpoint
bool ExpressLink::isOnboarded()
{
    // OK {status} {onboarded} [CONNECTED/DISCONNECTED] [STAGING/CUSTOMER]
    cmd("CONNECT?");
    return response.substring(2, 3) == "1";
}

/// @brief equivalent to: AT+DISCONNECT
/// @return true on success, false on error
bool ExpressLink::disconnect()
{
    return cmd("DISCONNECT");
}

/// @brief soft-reboot of the module, equivalent to: AT+RESET
/// @return true on success, false on error
bool ExpressLink::reset()
{
    return cmd("RESET");
}

/// @brief wipe all data and config, equivalent to: AT+FACTORY_RESET
/// @return true on success, false on error
bool ExpressLink::factoryReset()
{
    return cmd("FACTORY_RESET");
}

/// @brief Gets the next pending Event, if available
/// @param checkPin true (default) if the EVENT pin should be read; false if the AT+EVENT? command should be used
/// @return Event struct with event code and parameter, `code`==NONE if no event is pending
ExpressLink::Event ExpressLink::getEvent(bool checkPin)
{
    // response format:
    //   OK [{event_identifier} {parameter} {mnemonic [detail]}]{EOL}

    Event event;
    if (checkPin && eventPin >= 0 && digitalRead(eventPin) == LOW)
    {
        event.code = NONE;
        event.parameter = 0;
        return event;
    }
    if (!cmd("EVENT?"))
    {
        event.code = UNKNOWN;
        event.parameter = 0;
        return event;
    }
    if (response == "") // OK prefix has already been parsed by cmd()
    {
        event.code = NONE;
        event.parameter = 0;
        return event;
    }

    char *next;
    auto code = strtol(response.c_str(), &next, 10);
    if (code >= FIRST_EVENT_CODE && code < LAST_EVENT_CODE)
    {
        event.code = EventCode(strtol(response.c_str(), &next, 10));
    }
    else
    {
        event.code = UNKNOWN;
        event.parameter = 0;
        return event;
    }

    char *rest;
    event.parameter = strtol(next, &rest, 10);

    response = String(rest);
    response = response.substring(response.indexOf(' ')); // make optional `detail` available

    return event;
}

/// @brief Subscribe to Topic#.
///
/// Equivalent to `AT+CONF Topic{topic_index}={topic_name}` followed by `AT+SUBSCRIBE{topic_index}`.
/// @param topic_index index to subscribe to
/// @param topic_name name of topic (empty to skip setting topic in configuration dictionary)
/// @return true on success, false on error
bool ExpressLink::subscribe(uint8_t topic_index, String topic_name)
{
    if (topic_name.length() > 0)
    {
        config.setTopic(topic_index, topic_name);
    }
    return cmd("SUBSCRIBE" + String(topic_index));
}

/// @brief Unsubscribe from Topic#.
///
/// Equivalent to `AT+UNSUBSCRIBE{topic_index}`.
/// @param topic_index index to unsubscribe from
/// @return true on success, false on error
bool ExpressLink::unsubscribe(uint8_t topic_index)
{
    return cmd("UNSUBSCRIBE" + String(topic_index));
}

/// @brief Request next message pending on the indicated topic.
///
/// Equivalent to `AT+GET{topic_index}`.
/// @param topic_index use -1 (default) for `GET`, or value for `GETx`
/// @return true on success, false on error
bool ExpressLink::get(uint8_t topic_index)
{
    return cmd("GET" + String(topic_index));
}

/// @brief Same as `ExpressLink::publish - use it instead.`
/// @param topic_index
/// @param message
/// @return true on success, false on error
bool ExpressLink::send(uint8_t topic_index, String message)
{
    return publish(topic_index, message);
}

/// @brief Publish msg on a topic selected from topic list.
///
/// Equivalent to `AT+SEND{topic_index} {message}`.
/// @param topic_index the topic index to publish to
/// @param message raw message to publish, typically JSON-encoded
/// @return true on success, false on error
bool ExpressLink::publish(uint8_t topic_index, String message)
{
    return cmd("SEND" + String(topic_index) + " " + message);
}

/// @brief Fetches the current state of the OTA process.
///
/// Equivalent to `AT+OTA?`.
/// @return OTA state with code and detail
ExpressLink::OTAState ExpressLink::otaGetState()
{
    OTAState s;
    cmd("OTA?");
    s.code = OTACode(response.charAt(0) - '0'); // get numerical digit value from string character
    if (response.length() > 2)
    {
        s.detail = String(response.substring(2));
    }
    else
    {
        s.detail = "";
    }
    return s;
}

/// @brief Allow the OTA operation to proceed.
///
/// Equivalent to `AT+OTA ACCEPT<EOL>`.
/// @return true on success, false on error
bool ExpressLink::otaAccept()
{
    return cmd("OTA ACCEPT");
}

/// @brief Requests the next # bytes from the OTA buffer.
///
/// Equivalent to `AT+OTA READ {count}<EOL>`.
///
/// Retreive payload from `ExpressLink::response`
/// @param count decimal value of number of bytes to read
/// @return true on success, false on error
bool ExpressLink::otaRead(uint32_t count)
{
    return cmd("OTA READ " + String(count));
}

/// @brief Moves the read pointer to an absolute address.
///
/// Equivalent to `AT+OTA SEEK<EOL>` or `AT+OTA SEEK {address}<EOL>`.
/// @param address decimal value for read pointer to seek to, or -1 to seek to beginning
/// @return true on success, false on error
bool ExpressLink::otaSeek(uint32_t address)
{
    if (address == (uint32_t)-1)
    {
        return cmd("OTA SEEK");
    }
    else
    {
        return cmd("OTA SEEK " + String(address));
    }
}

/// @brief Authorize the ExpressLink module to apply the new image.
///
/// Equivalent to `AT+OTA APPLY<EOL>`.
/// @return true on success, false on error
bool ExpressLink::otaApply()
{
    return cmd("OTA APPLY");
}

/// @brief The host OTA operation is completed.
///
/// Equivalent to `AT+OTA CLOSE<EOL>`.
/// @return true on success, false on error
bool ExpressLink::otaClose()
{
    return cmd("OTA CLOSE");
}

/// @brief The contents of the OTA buffer are emptied.
///
/// Equivalent to `AT+OTA FLUSH<EOL>`.
/// @return true on success, false on error
bool ExpressLink::otaFlush()
{
    return cmd("OTA FLUSH");
}

/// @brief Initialize communication with the Device Shadow service.
///
/// Equivalent to `AT+SHADOW{index} INIT<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowInit(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " INIT");
}

/// @brief Request a Device Shadow document.
///
/// Equivalent to `AT+SHADOW{index} DOC<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowDoc(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " DOC");
}

/// @brief Retrieve a device shadow document.
///
/// Equivalent to `AT+SHADOW{index} GET DOC<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowGetDoc(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " GET DOC");
}

/// @brief Request a device shadow document update.
///
/// Equivalent to `AT+SHADOW{index} UPDATE {new_state}<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowUpdate(String new_state, uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " UPDATE " + new_state);
}

/// @brief Retrieve a device shadow update response.
///
/// Equivalent to `AT+SHADOW{index} GET UPDATE<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowGetUpdate(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " GET UPDATE");
}

/// @brief Subscribe to a device shadow document.
///
/// Equivalent to `AT+SHADOW{index} SUBSCRIBE<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowSubscribe(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " SUBSCRIBE");
}

/// @brief Unsubscribe from a device shadow document.
///
/// Equivalent to `AT+SHADOW{index} UNSUBSCRIBE<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowUnsubscribe(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " UNSUBSCRIBE");
}

/// @brief Retrieve a Shadow Delta message.
///
/// Equivalent to `AT+SHADOW{index} GET DELTA<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowGetDelta(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " GET DELTA");
}

/// @brief Request the deletion of a Shadow document.
///
/// Equivalent to `AT+SHADOW{index} DELETE<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowDelete(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " DELETE");
}

/// @brief Request a Shadow delete response.
///
/// Equivalent to `AT+SHADOW{index} GET DELETE<EOL>`.
/// @param index shadow index. Use -1 (default), to select the unnamed shadow.
/// @return true on success, false on error
bool ExpressLink::shadowGetDelete(uint8_t index)
{
    String i = (index >= 0) ? String(index) : "";
    return cmd("SHADOW" + i + " GET DELETE");
}

/// @brief Enters Serial/UART passthrough mode.
/// All serial communication is bridged between the ExpressLink UART and the passed `destination`.
/// This function never returns. You can use it for debugging or over-the-wire firmware upgrades.
void ExpressLink::passthrough(Stream *destination)
{
    // inspired by https://docs.arduino.cc/built-in-examples/communication/SerialPassthrough
    while (true)
    {
        if (destination->available())
        {
            uart->write(destination->read());
        }
        if (uart->available())
        {
            destination->write(uart->read());
        }
    }
}