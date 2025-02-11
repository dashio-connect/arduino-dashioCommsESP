#include <Arduino.h>
#include <DashioESP.h>
#include <DashioProvisionESP.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <driver/rtc_io.h>

enum CommsBoardType {
    BOARD_ARDUINO,
    BOARD_ARDUINO_COMMS,
    BOARD_DASH_DEVICE_COMMS,
    BOARD_DASH_DEVICE_MINI
};

/*???
#ifdef CONFIG_IDF_TARGET_ESP32S3
struct CommsModuleConfig {
    CommsBoardType commsBoardType = BOARD_ARDUINO
    
    // Wakeup from sleep EXT1 pin
    gpio_num_t extWakeupPin = GPIO_NUM_1;

    gpio_num_t buttonPin = GPIO_NUM_3;

    // Leds
    bool ledOnIsLow = true;
    gpio_num_t ledPinWiFi = GPIO_NUM_4;
    gpio_num_t ledPinMQTT = GPIO_NUM_5;
    gpio_num_t ledPinTCP = GPIO_NUM_6;
    gpio_num_t ledPinBLE = GPIO_NUM_7;
    bool enableLEDtest = true;

    // Serial
    int baudRate = 115200;
    gpio_num_t serialTx = GPIO_NUM_43;
    gpio_num_t serialRx = GPIO_NUM_44;
    HardwareSerial *uart = &Serial2;

    // Buffers
    uint16_t messageBufferSize = 10000;

    // Dash Sensor IO Board
    gpio_num_t sensorIOenable = GPIO_NUM_NC;
};
#else
*/
struct CommsModuleConfig {
    CommsBoardType commsBoardType = BOARD_ARDUINO;

    // Wakeup from sleep EXT1 pin
    gpio_num_t extWakeupPin = GPIO_NUM_NC;

    gpio_num_t buttonPin = GPIO_NUM_NC;

    // Leds
    bool ledOnIsLow = true;
    gpio_num_t ledPinWiFi = GPIO_NUM_NC;
    gpio_num_t ledPinMQTT = GPIO_NUM_NC;
    gpio_num_t ledPinTCP = GPIO_NUM_NC;
    gpio_num_t ledPinBLE = GPIO_NUM_NC;
    bool enableLEDtest = true;

    // Serial
    int baudRate = 115200;
    gpio_num_t serialTx = GPIO_NUM_NC;
    gpio_num_t serialRx = GPIO_NUM_NC;
    HardwareSerial *uart = NULL;

    // Buffers
    uint16_t messageBufferSize = 10000;

    // Dash Sensor IO Board
    gpio_num_t sensorIOenable = GPIO_NUM_NC;
};
//???#endif


// Defines
#define MIN_BLE_TIMEOUT 20
#define MAX_WORD 64
#define MAX_BUFFER_SIZE 10000

// LED Defines
#define MAX_LED_STATES 8
const uint8_t LED_STATE_OFF = 0;
const uint8_t LED_STATE_STARTUP = 1;
const uint8_t LED_STATE_SEARCHING = 4;
const uint8_t LED_STATE_CONNECTED = MAX_LED_STATES;

#define deviceC64key "c64" // Not in provisioning, but can be set by master

//command list
const char WHO[] = "WHO";
const int WHOLEN = 3;
const char DEVICE[] = "DVCE";
const int DEVICELEN = 4;
const char WIFI[] = "WIFI";
const int WIFILEN  = 4;
const char TCP[] = "TCP";
const int TCPLEN = 3;
const char MQTT[] = "MQTT";
const int MQTTLEN = 4;
const char BLE[] = "BLE";
const int BLELEN = 3;
const char ALL[] = "ALL";
const int ALLLEN = 3;
const char CTRL[] = "CTRL";
const int CTRLLEN = 4;
const char CNCTN[] = "CNCTN";
const int CNCTNLEN = 5;
const char REBOOT[] = "REBOOT";
const int REBOOTLEN = 6;
const char SLEEP[] = "SLEEP";
const int SLEEPLEN = 5;
const char INIT[] = "INIT";
const int INITLEN = 4;
const char CFG[] = "CFG";
const int CFGLEN = 3;
const char C64[] = "C64";
const int C64LEN = 3;
const char EN[] = "EN";
const int ENLEN = 2;
const char HALT[] = "HLT";
const int HALTLEN = 3;
const char STE[] = "STE";
const int STELEN = 3;
const char ALM[] = "ALM";
const int ALMLEN = 3;
const char CLK[] = "CLK";
const int CLKLEN = 3;
const char LED[] = "LED";
const int LEDLEN = 3;

const char DELIM_STR[] = "\t";
const char END_DELIM_STR[] = "\n";
const char DELIMETERS_STR[] = "\t\n";

enum CommsModuleMode {
    MODULE_MODE_DASH_DEVICE,
    MODULE_MODE_DASH_SERIAL
};

class DashCommsESP {
public:    
    static CommsModuleConfig config;

    static DashDevice *dashDevice;
    static DashProvision *provisioning;
    static DashWiFi *wifi;
    static DashMQTT *mqtt_con;
    static DashTCP *tcp_con;
    static DashBLE *ble_con;

    char currentChar = 0;

    static bool isWiFiRunning;
    static bool isBLE;
    static bool isTCP;
    static bool isMQTT;

    DashCommsESP();
    DashCommsESP(const char *type, const char *name);
    DashCommsESP(const char *type, const char *name, const char *configC64Str, unsigned int cfgRevision);

    static void sendControlMessage(const char* controlID = nullptr, const char* Payload = nullptr);
    static void forwardMessageToSerial(MessageData *messageData);

    void sendMessageAll(const String& message);
    static void sendMessage(const String& message, ConnectionType connectionType);

    static bool timerStopBLE(void *opaque);

    void init(uint8_t numBLE, uint8_t numTCP, bool dashMQTT, void (*_processIncomingMessage)(MessageData *messageData) = nullptr);
    static void setBLEtimeout(uint16_t timeout);
    static void setLEDsTurnoff(uint16_t timeout);
    void setBoardType(CommsBoardType boardType);
    void setBLEpassKey(uint32_t passKey);
    void begin();
    void run();

    void enableRebootAlarm(bool enable);
    void sendAlarm(const String& message);
    void addDashStore(ControlType controlType, String controlID);

private:
    static bool initDone;

    uint16_t mqttPORT;

    static uint8_t uiStartupSequenceCounter;

    static bool bleSwEnabled;
    static uint16_t bleButtonTimeoutS;
    static uint32_t bleCountdown;
    static uint8_t buttonPressCount;

    static bool ledsEnabled;
    static uint16_t ledsOffTimeoutS;
    static uint16_t ledsOffCountdown;

    static uint8_t ledTimerCount;

    static CommsModuleMode moduleMode;
    static bool serialInitDone;
    static uint8_t sendRebootCount;

    static void (*processIncomingMessage)(MessageData *messageData);
    static void interceptIncomingMessage(MessageData *messageData);

    int serialRecieveBufferIndex = 0;
    char *serialReceiveBuffer;
    char *serialTransmitBuffer;
    char *messageBuffer; // For incoming messages
    char *configC64; // For storing the config to memory when provided by the master

    void setHardwareConfig();
    static void onProvisionCallback(ConnectionType connectionType, const String& message, bool commsChanged);
    static void statusCallback(StatusCode statusCode);
    void parseMessage();
    void sendNmlMessage(char *token, char *deviceID, ConnectionType connectionType);

    static void startBLE();
    static void stopBLE();
    static void startWiFi(bool alowRestart = false);
    static void stopWiFi();
    static void startTCP();
    static void stopTCP();
    static void startMQTT();
    static void stopMQTT();
    static void sleep();

    static void updateLED(uint8_t pin, uint8_t state);
    static void userInterfaceTask(void *parameters);
};
