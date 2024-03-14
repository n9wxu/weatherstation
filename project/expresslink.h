#ifndef _EXPRESSLINK_
#define _EXPRESSLINK_

#include <stdint.h>

typedef enum response_codes
{
    EL_NORESPONSE = -1,
    EL_OK = 0,
    EL_OVERFLOW,
    EL_PARSE_ERROR,
    EL_COMMAND_NOT_FOUND,
    EL_PARAMETER_ERROR,
    EL_INVALID_ESCAPE,
    EL_NO_CONNECTION,
    EL_TOPIC_OUT_OF_RANGE,
    EL_TOPIC_UNDEFINED,
    EL_INVALID_KEY_LENGTH,
    EL_INVALID_KEY_NAME,
    EL_UNKNOWN_KEY,
    EL_KEY_READONLY,
    EL_KEY_WRITEONLY,
    EL_UNABLE_TO_CONNECT,
    EL_TIME_NOT_AVAILABLE,
    EL_LOCATION_NOT_AVAILABLE,
    EL_MODE_NOT_AVAILABLE,
    EL_ACTIVE_CONNECTION,
    EL_HOST_IMAGE_NOT_AVAILABLE,
    EL_INVALID_ADDRESS,
    EL_INVALID_OTA_UPDATE,
    EL_INVALID_QUERY,
    EL_INVALID_SIGNATURE
} response_codes_t;

response_codes_t expresslinkSendCommand(const char *command, char *response, size_t responseLength);
void expresslinkConnect();
void expresslinkDisconnect();
void expresslinkInit();
void expresslinkPublish(int topic, char *message, size_t messageLength);
void expresslinkGetThingName(char *thingName, size_t thingNameLen);

#endif //_EXPRESSLINK_