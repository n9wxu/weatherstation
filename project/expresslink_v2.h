#ifndef _EXPRESSLINK_V2
#define _EXPRESSLINK_V2
#include <stdint.h>

ExpressLinkConfig_init(ExpressLink &el);

char *ExpressLinkConfig_getAbout();
char *ExpressLinkConfig_getVersion();
char *ExpressLinkConfig_getTechSpec();
char *ExpressLinkConfig_getThingName();
char *ExpressLinkConfig_getCertificate();

char *ExpressLinkConfig_getCustomName();
bool ExpressLinkConfig_setCustomName(const char *value);

char *ExpressLinkConfig_getEndpoint();
bool ExpressLinkConfig_setEndpoint(const char *value);

char *ExpressLinkConfig_getRootCA();
bool ExpressLinkConfig_setRootCA(const char *value);

char *ExpressLinkConfig_getShadowToken();
bool ExpressLinkConfig_setShadowToken(const char *value);

uint32_t ExpressLinkConfig_getDefenderPeriod();
bool ExpressLinkConfig_setDefenderPeriod(const uint32_t value);

char *ExpressLinkConfig_getHOTAcertificate();
bool ExpressLinkConfig_setHOTAcertificate(const char *value);

char *ExpressLinkConfig_getOTAcertificate();
bool ExpressLinkConfig_setOTAcertificate(const char *value);

char *ExpressLinkConfig_getSSID();
bool ExpressLinkConfig_setSSID(const char *value);

bool ExpressLinkConfig_setPassphrase(const char *value);

char *ExpressLinkConfig_getAPN();
bool ExpressLinkConfig_setAPN(const char *value);

char *ExpressLinkConfig_getTopic(uint8_t index);
bool ExpressLinkConfig_setTopic(uint8_t index, char *topic);

char *ExpressLinkConfig_getShadow(uint8_t index);
bool ExpressLinkConfig_setShadow(uint8_t index, char *topic);

/// @brief see https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-event-handling.html#elpg-event-handling-commands
enum EventCode
{
    UNKNOWN = -2, /// Failed to parse response, Check `response` for full event. Not defined in ExpressLink TechSpec.
    NONE = -1,    /// No event retrieved, queue was empty. Not defined in ExpressLink TechSpec.
    FIRST_EVENT_CODE = 1,
    MSG = 1,      /// A message was received on the topic #. Parameter=Topic Index
    STARTUP = 2,  /// The module has entered the active state. Parameter=0
    CONLOST = 3,  /// Connection unexpectedly lost. Parameter=0
    OVERRUN = 4,  /// Receive buffer Overrun (topic in detail). Parameter=0
    OTA = 5,      /// OTA event (see the OTA? command for details). Parameter=0
    CONNECT = 6,  /// A connection was established or failed. Parameter=Connection Hint
    CONFMODE = 7, /// CONFMODE exit with success. Parameter=0
    SUBACK = 8,   /// A subscription was accepted. Parameter=Topic Index
    SUBNACK = 9,  /// A subscription was rejected. Parameter=Topic Index
    // 10..19 RESERVED
    SHADOW_INIT = 20,        /// Shadow[Shadow Index] interface was initialized successfully. Parameter=Shadow Index
    SHADOW_INIT_FAILED = 21, /// The SHADOW[Shadow Index] interface initialization failed. Parameter=Shadow Index
    SHADOW_DOC = 22,         /// A Shadow document was received. Parameter=Shadow Index
    SHADOW_UPDATE = 23,      /// A Shadow update result was received. Parameter=Shadow Index
    SHADOW_DELTA = 24,       /// A Shadow delta update was received. Parameter=Shadow Index
    SHADOW_DELETE = 25,      /// A Shadow delete result was received. Parameter=Shadow Index
    SHADOW_SUBACK = 26,      /// A Shadow delta subscription was accepted. Parameter=Shadow Index
    SHADOW_SUBNACK = 27,     /// A Shadow delta subscription was rejected. Parameter=Shadow Index
    LAST_EVENT_CODE,
    // <= 999 reserved
    // >= 1000 available for custom implementation
};

/// @brief see https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-ota-updates.html#elpg-ota-commands
enum OTACode
{
    NoOTAInProgress = 0,          /// No OTA in progress.
    UpdateProposed = 1,           /// A new module OTA update is being proposed. The host can inspect the version number and decide to accept or reject it. The {detail} field provides the version information (string).
    HostUpdateProposed = 2,       /// A new Host OTA update is being proposed. The host can inspect the version details and decide to accept or reject it. The {detail} field provides the metadata that is entered by the operator (string).
    OTAInProgress = 3,            /// OTA in progress. The download and signature verification steps have not been completed yet.
    NewExpressLinkImageReady = 4, /// A new module firmware image has arrived. The signature has been verified and the ExpressLink module is ready to reboot. (Also, an event was generated.)
    NewHostImageReady = 5,        /// A new host image has arrived. The signature has been verified and the ExpressLink module is ready to read its contents to the host. The size of the file is indicated in the response detail. (Also, an event was generated.)
};

struct Event
{
    EventCode code;
    int parameter;
};

struct OTAState
{
    OTACode code;
    String detail;
};

ExpressLink_init(void);

// bool begin(Stream &s, int event = -1, int wake = -1, int reset = -1, bool debug = false);

bool ExpressLink_cmd(String command);

bool ExpressLink_selfTest();

bool ExpressLink_connect(bool async = false);
bool ExpressLink_isConnected();
bool ExpressLink_isOnboarded();
bool ExpressLink_disconnect();

bool ExpressLink_reset();
bool ExpressLink_factoryReset();
bool ExpressLink_sleep(uint32_t duration, uint8_t sleep_mode = 0);

Event ExpressLink_getEvent(bool checkPin = true);

bool ExpressLink_subscribe(uint8_t topic_index, String topic_name);
bool ExpressLink_unsubscribe(uint8_t topic_index);
bool ExpressLink_get(uint8_t topic_index = -1); // -1 = GET, 0...X = GETX
bool ExpressLink_send(uint8_t topic_index, String message);
bool ExpressLink_publish(uint8_t topic_index, String message);

OTAState ExpressLink_otaGetState();
bool ExpressLink_otaAccept();
bool ExpressLink_otaRead(uint32_t count);
bool ExpressLink_otaSeek(uint32_t address = -1);
bool ExpressLink_otaApply();
bool ExpressLink_otaClose();
bool ExpressLink_otaFlush();

bool ExpressLink_shadowInit(uint8_t index = -1);
bool ExpressLink_shadowDoc(uint8_t index = -1);
bool ExpressLink_shadowGetDoc(uint8_t index = -1);
bool ExpressLink_shadowUpdate(String new_state, uint8_t index = -1);
bool ExpressLink_shadowGetUpdate(uint8_t index = -1);
bool ExpressLink_shadowSubscribe(uint8_t index = -1);
bool ExpressLink_shadowUnsubscribe(uint8_t index = -1);
bool ExpressLink_shadowGetDelta(uint8_t index = -1);
bool ExpressLink_shadowDelete(uint8_t index = -1);
bool ExpressLink_shadowGetDelete(uint8_t index = -1);

void ExpressLink_passthrough(Stream *destination);

ExpressLinkConfig ExpressLink_config;

#endif // _EXPRESSLINK_V2