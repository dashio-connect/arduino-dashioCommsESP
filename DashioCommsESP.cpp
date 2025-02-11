#include <dashioCommsESP.h>
#include <dashio.h>
#include <HardwareSerial.h>

Preferences credentials;

CommsModuleConfig DashCommsESP::config;

DashDevice *DashCommsESP::dashDevice = nullptr;
DashProvision *DashCommsESP::provisioning = nullptr;
DashWiFi *DashCommsESP::wifi = nullptr;
DashMQTT *DashCommsESP::mqtt_con = nullptr;
DashTCP *DashCommsESP::tcp_con = nullptr;
DashBLE *DashCommsESP::ble_con = nullptr;

void (*DashCommsESP::processIncomingMessage)(MessageData *messageData);

bool DashCommsESP::initDone = false;

uint8_t DashCommsESP::uiStartupSequenceCounter = 0;

uint8_t DashCommsESP::buttonPressCount = 0;
uint16_t DashCommsESP::bleButtonTimeoutS = 0;
uint32_t DashCommsESP::bleCountdown = 0;
bool DashCommsESP::bleSwEnabled = false;

bool DashCommsESP::ledsEnabled = true;
uint16_t DashCommsESP::ledsOffTimeoutS = 0;
uint16_t DashCommsESP::ledsOffCountdown = 0;

bool DashCommsESP::isWiFiRunning = false;
bool DashCommsESP::isBLE = false;
bool DashCommsESP::isTCP = false;
bool DashCommsESP::isMQTT = false;

CommsModuleMode DashCommsESP::moduleMode = MODULE_MODE_DASH_DEVICE;
bool DashCommsESP::serialInitDone = false;
uint8_t DashCommsESP::sendRebootCount = 0;

uint8_t DashCommsESP::ledTimerCount = 0;


DashCommsESP::DashCommsESP() {
    moduleMode = MODULE_MODE_DASH_SERIAL;

    dashDevice = new DashDevice(String("Comms Module Type"));

    dashDevice->name = "";
}

DashCommsESP::DashCommsESP(const char *type, const char *name) {
    DashCommsESP(type, name, NULL, 0);
}

DashCommsESP::DashCommsESP(const char *type, const char *name, const char *configC64Str, unsigned int cfgRevision) {
    moduleMode = MODULE_MODE_DASH_DEVICE;

    dashDevice = new DashDevice(type, configC64Str, cfgRevision);

    if (strlen(name) > 0) {
        dashDevice->name = String(name);
    }
}

void DashCommsESP::setHardwareConfig() {
    if (config.commsBoardType != BOARD_ARDUINO) {
#if defined(ARDUINO_ESP32S3_DEV)
        esp_sleep_enable_ext1_wakeup(1ULL << config.extWakeupPin, ESP_EXT1_WAKEUP_ANY_LOW);
#else
        esp_sleep_enable_ext1_wakeup(1ULL << config.extWakeupPin, ESP_EXT1_WAKEUP_ALL_LOW);
#endif
        rtc_gpio_pullup_en(config.extWakeupPin);
        rtc_gpio_pulldown_dis(config.extWakeupPin);
        
        // LEDs (and turn off)
        pinMode(config.ledPinWiFi, OUTPUT);
        digitalWrite(config.ledPinWiFi, config.ledOnIsLow);
        
        pinMode(config.ledPinBLE, OUTPUT);
        digitalWrite(config.ledPinBLE, config.ledOnIsLow);
        
        pinMode(config.ledPinTCP, OUTPUT);
        digitalWrite(config.ledPinTCP, config.ledOnIsLow);
        
        pinMode(config.ledPinMQTT, OUTPUT);
        digitalWrite(config.ledPinMQTT, config.ledOnIsLow);
        
        // Push Button
        if (config.buttonPin != GPIO_NUM_NC) {
            pinMode(config.buttonPin, INPUT); //??? Currently has an external pullup, so don't need INPUT_PULLUP
        }
    }
}

void DashCommsESP::setBoardType(CommsBoardType boardType) {
    config.commsBoardType = boardType;
    
    if (boardType == BOARD_DASH_DEVICE_MINI) {
        config.extWakeupPin = GPIO_NUM_NC;
        
        config.buttonPin = GPIO_NUM_1;
        
        // LEDs
        config.ledPinWiFi = GPIO_NUM_NC;
        config.ledPinMQTT = GPIO_NUM_2;
        config.ledPinTCP = GPIO_NUM_3;
        config.ledPinBLE = GPIO_NUM_4;
        
        // Dash Sensor IO Board
        config.sensorIOenable = GPIO_NUM_21;
    } else if (boardType == BOARD_DASH_DEVICE_COMMS) {
#ifdef CONFIG_IDF_TARGET_ESP32S3
        // Wakeup from sleep EXT1 pin
        config.extWakeupPin = GPIO_NUM_1;

        config.buttonPin = GPIO_NUM_3;

        // Leds
        config.ledPinWiFi = GPIO_NUM_4;
        config.ledPinMQTT = GPIO_NUM_5;
        config.ledPinTCP = GPIO_NUM_6;
        config.ledPinBLE = GPIO_NUM_7;

        // Serial
        config.serialTx = GPIO_NUM_43;
        config.serialRx = GPIO_NUM_44;
#endif
    } else if (boardType == BOARD_ARDUINO_COMMS) {
        // Serial
        gpio_num_t serialTx = GPIO_NUM_17;
        gpio_num_t serialRx = GPIO_NUM_16;
    }
}

void DashCommsESP::init(uint8_t numBLE, uint8_t numTCP, bool dashMQTT, void (*_processIncomingMessage)(MessageData *messageData)) {
    processIncomingMessage = _processIncomingMessage;

    if (!initDone) {
        dashDevice->setup(Network.macAddress());

        initDone = true;
        dashDevice->statusCallback = &statusCallback;
        setHardwareConfig();

        if (moduleMode != MODULE_MODE_DASH_DEVICE) {
            serialReceiveBuffer = new char[MAX_BUFFER_SIZE];
            serialTransmitBuffer = new char[MAX_BUFFER_SIZE];
            messageBuffer = new char[MAX_BUFFER_SIZE];
            configC64 = new char[MAX_BUFFER_SIZE];
        }

        // Setup task scheduler for LEDs etc.
        xTaskCreate(userInterfaceTask, "uiTask", 4096, NULL, 1, NULL); //??? parameters = this?

        provisioning = new DashProvision(dashDevice);
        provisioning->load(onProvisionCallback);

        if ((numTCP > 0) or dashMQTT) {
            wifi = new DashWiFi(dashDevice);

            if (numTCP > 0) {
                tcp_con = new DashTCP(dashDevice, true, provisioning->tcpPort, numTCP); 
                tcp_con->setCallback(&interceptIncomingMessage);
            }
            if (dashMQTT) {
                mqtt_con = new DashMQTT(dashDevice, false, true);
                mqtt_con->esp32_mqtt_blocking = false;
                mqtt_con->setCallback(&interceptIncomingMessage);
            }
        }

        if (numBLE > 0) {
            ble_con = new DashBLE(dashDevice, true, numBLE);
            ble_con->setCallback(&interceptIncomingMessage);
        }
    }
}

void DashCommsESP::interceptIncomingMessage(MessageData *messageData) {
    switch (messageData->control) {
    case deviceName: case wifiSetup: case dashioSetup: case tcpSetup:
        provisioning->processMessage(messageData);
        break;
    default:
        if (moduleMode == MODULE_MODE_DASH_DEVICE) {
            processIncomingMessage(messageData);
        } else if (moduleMode == MODULE_MODE_DASH_SERIAL) {
            forwardMessageToSerial(messageData);
        }
        break;
    } 
}

void DashCommsESP::statusCallback(StatusCode statusCode) {
    if (statusCode == wifiConnected) {
        sendControlMessage(WIFI, EN);
    } else if (statusCode == wifiDisconnected) {
        sendControlMessage(WIFI, HALT);
    } else if (statusCode == mqttConnected) {
        sendControlMessage(MQTT, EN);
    } else if (statusCode == mqttDisconnected) {
        sendControlMessage(MQTT, HALT);
    }
}

void DashCommsESP::onProvisionCallback(ConnectionType connectionType, const String& message, bool commsChanged) {
    sendMessage(message, connectionType);

    if (commsChanged) {
        if (mqtt_con != nullptr) {
            mqtt_con->setup(provisioning->dashUserName, provisioning->dashPassword);
        }
        startWiFi(true);
    } else {
        if (mqtt_con != nullptr) {
            mqtt_con->sendWhoAnnounce();
        }
    }
}

void DashCommsESP::sendMessageAll(const String& message) {
    if (ble_con != nullptr) {
        ble_con->sendMessage(message);
    }
    if (tcp_con != nullptr) {
        tcp_con->sendMessage(message);
    }
    if (mqtt_con != nullptr) {
        mqtt_con->sendMessage(message);
    }
}

void DashCommsESP::sendMessage(const String& message, ConnectionType connectionType) {
    if (connectionType == BLE_CONN) {
        if (isBLE){
            if (ble_con != nullptr) {
                ble_con->sendMessage(message);
            }
        }
    } else if (connectionType == TCP_CONN) {
        if (isTCP) {
            if (tcp_con != nullptr) {
                tcp_con->sendMessage(message);
            }
        }
    } else if (connectionType == MQTT_CONN) {
        if (isMQTT) {
            if (mqtt_con != nullptr) {
                mqtt_con->sendMessage(message);
            }
        }
    } else if (connectionType == ALL_CONN) {
        if (isBLE){
            if (ble_con != nullptr) {
                ble_con->sendMessage(message);
            }
        }
        if (isTCP) {
            if (tcp_con != nullptr) {
                tcp_con->sendMessage(message);
            }
        }
        if (isMQTT) {
            if (mqtt_con != nullptr) {
                mqtt_con->sendMessage(message);
            }
        }
    }
}

void DashCommsESP::enableRebootAlarm(bool enable) {
    if (mqtt_con != nullptr) {
        mqtt_con->sendRebootAlarm = enable && (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1); // i.e. don't sent alarm if waking
    }
}

void DashCommsESP::sendAlarm(const String& message) {
    if (mqtt_con != nullptr) {
    }
}

void DashCommsESP::addDashStore(ControlType controlType, String controlID) {
    if (mqtt_con != nullptr) {
        mqtt_con->addDashStore(controlType, controlID);
    }
}

void DashCommsESP::begin() {
    if (moduleMode == MODULE_MODE_DASH_DEVICE) {
        setBLEtimeout(bleButtonTimeoutS);
        if (bleButtonTimeoutS > 0) {
            bleSwEnabled = true;
        }
        if (!bleSwEnabled) {
            startBLE();
        }
        startTCP();
        startMQTT();
    } else {
        // Serial begin
        config.uart->setRxBufferSize(MAX_BUFFER_SIZE);
        config.uart->begin(config.baudRate, SERIAL_8N1, config.serialRx, config.serialTx);
        config.uart->flush();
        config.uart->print(END_DELIM_STR);
    }
}

void DashCommsESP::sendControlMessage(const char* controlID, const char* payload) {
    if (moduleMode != MODULE_MODE_DASH_DEVICE) {
        int devIDlen = dashDevice->deviceID.length() + 1;

        char deviceID[devIDlen + 1];
        dashDevice->deviceID.toCharArray(deviceID, devIDlen);

        char str[devIDlen + 60];
        strcpy(str, DELIM_STR);
        strcat(str, deviceID);
        strcat(str, DELIM_STR);
        strcat(str, CTRL);

        if (controlID != nullptr) {
            strcat(str, DELIM_STR);
            strcat(str, controlID);
        }
        if (payload != nullptr) {
            strcat(str, DELIM_STR);
            strcat(str, payload);
        }
        strcat(str, END_DELIM_STR);

        Serial.print("Outgoing->");
        Serial.println(str);

        config.uart->print(str);
    }
}

void DashCommsESP::forwardMessageToSerial(MessageData *messageData) {
    String controlStr = dashDevice->getControlTypeStr(messageData->control);    
    String message = String(DELIM);
    message += messageData->getConnectionTypeStr(); // Prefix message with connectionn type
    message += messageData->getMessageGeneric(controlStr);

    Serial.print("Serial Forward->");
    Serial.println(message);

    config.uart->print(message);
}

void DashCommsESP::userInterfaceTask(void *parameters) {
    while(1) {
        // Manage BLE button
        if ((config.buttonPin != GPIO_NUM_NC) && bleSwEnabled) {
            if (!digitalRead(config.buttonPin)) { // i.e. button pressed
                if (buttonPressCount == 1) {
                    if ((!ledsEnabled) && (ledsOffTimeoutS > 0)) {
                        setLEDsTurnoff(ledsOffTimeoutS);
                    } else if (ble_con != nullptr) {
                        if (isBLE) {
                            stopBLE();
                        } else {
                            setBLEtimeout(bleButtonTimeoutS);
                            startBLE();
                        }
                    }
                }
                if (buttonPressCount <= 1) {
                    buttonPressCount += 1;
                }
            } else {
                buttonPressCount = 0;
            }
        }

        ledTimerCount++;
        if (ledTimerCount >= MAX_LED_STATES) { // i.e. Every 1/2 second
            ledTimerCount = 0;

            // Send serial REBOOT message when
            if (moduleMode == MODULE_MODE_DASH_SERIAL) {
                sendRebootCount++;
                if (sendRebootCount > 1) {
                    sendRebootCount = 0;
                    if (!serialInitDone) {
                        sendControlMessage(REBOOT);
                    }
                }
            }

            // Check BLE turnoff counter
            if ((isBLE) && (bleCountdown > 0)) {
                bleCountdown--;
                if (bleCountdown <= 0) {
                    if (ble_con != nullptr) {
                        if (ble_con->isConnected()) {
                            bleCountdown -= 1;
                        } else {
                            stopBLE();
                        }
                    }
                }
            }

            // Check LEDs turnoff counter
            if (ledsOffCountdown > 0) {
                ledsOffCountdown--;
                if (ledsOffCountdown <= 0) {
                    ledsEnabled = false;
                }
            }
        }

        if ((config.enableLEDtest) && (uiStartupSequenceCounter < 16)) {
            switch (uiStartupSequenceCounter) {
                case 0:
                    if (config.ledPinWiFi != GPIO_NUM_NC) {
                        digitalWrite(config.ledPinWiFi, !config.ledOnIsLow);
                    }
                    break;
                case 2: 
                    if (config.ledPinMQTT != GPIO_NUM_NC) {
                        digitalWrite(config.ledPinMQTT, !config.ledOnIsLow);
                    }
                    break;
                case 4:
                    if (config.ledPinWiFi != GPIO_NUM_NC) {
                        digitalWrite(config.ledPinWiFi, config.ledOnIsLow);
                    }
                    if (config.ledPinTCP != GPIO_NUM_NC) {
                        digitalWrite(config.ledPinTCP, !config.ledOnIsLow);
                    }
                    break;
                case 6:
                    if (config.ledPinMQTT != GPIO_NUM_NC) {
                        digitalWrite(config.ledPinMQTT, config.ledOnIsLow);
                    }
                    if (config.ledPinBLE != GPIO_NUM_NC) {
                        digitalWrite(config.ledPinBLE, !config.ledOnIsLow);
                    }
                    break;
                case 8:
                    if (config.ledPinTCP != GPIO_NUM_NC) {
                        digitalWrite(config.ledPinTCP, config.ledOnIsLow);
                    }
                    break;
                case 10:
                    if (config.ledPinBLE != GPIO_NUM_NC) {
                        digitalWrite(config.ledPinBLE, config.ledOnIsLow);
                    }
                    break;
                default:
                    break;
            }
            uiStartupSequenceCounter++;
        } else if (ledsEnabled) {
            if (wifi == nullptr) {
                updateLED(config.ledPinWiFi, LED_STATE_OFF);
                updateLED(config.ledPinMQTT, LED_STATE_OFF);
                updateLED(config.ledPinTCP, LED_STATE_OFF);
            } else {
                uint8_t ledWiFi_state = LED_STATE_OFF;
                if (!isWiFiRunning) {
                    ledWiFi_state = LED_STATE_OFF;
                } else if (WiFi.status() == WL_CONNECTED) {
                    ledWiFi_state = LED_STATE_CONNECTED;
                } else {
                    ledWiFi_state = LED_STATE_SEARCHING;
                }

                updateLED(config.ledPinWiFi, ledWiFi_state);
                if (ledWiFi_state == LED_STATE_OFF) {
                    updateLED(config.ledPinMQTT, LED_STATE_OFF);
                    updateLED(config.ledPinTCP, LED_STATE_OFF);
                } else {
                    if (mqtt_con != nullptr) {
                        uint8_t ledMQTT_state = LED_STATE_OFF;
                        if (ledWiFi_state == LED_STATE_SEARCHING) {
                            ledMQTT_state = LED_STATE_STARTUP;
                        } else if (mqtt_con->state == subscribed) {
                            ledMQTT_state = LED_STATE_CONNECTED;
                        } else {
                            ledMQTT_state = LED_STATE_SEARCHING;
                        }
                        updateLED(config.ledPinMQTT, ledMQTT_state);
                    } else {
                        updateLED(config.ledPinMQTT, LED_STATE_OFF);
                    }

                    if (tcp_con != nullptr) {
                        uint8_t ledTCP_state = LED_STATE_OFF;
                        if (ledWiFi_state == LED_STATE_SEARCHING) {
                            ledTCP_state = LED_STATE_STARTUP;
                        } else if (tcp_con->hasClient()) {
                            ledTCP_state = LED_STATE_CONNECTED;
                        } else {
                            ledTCP_state = LED_STATE_SEARCHING;
                        }
                        updateLED(config.ledPinTCP, ledTCP_state);
                    } else {
                        updateLED(config.ledPinTCP, LED_STATE_OFF);
                    }
                }
            }

            if (ble_con != nullptr) {
                uint8_t ledBLE_state = LED_STATE_OFF;
                if (!isBLE) {
                    ledBLE_state = LED_STATE_OFF;
                } else if (ble_con->isConnected()) {
                    ledBLE_state = LED_STATE_CONNECTED;
                } else {
                    ledBLE_state = LED_STATE_SEARCHING;
                }
                updateLED(config.ledPinBLE, ledBLE_state);
            } else {
                updateLED(config.ledPinBLE, LED_STATE_OFF);
            }
        } else {
            digitalWrite(config.ledPinWiFi, config.ledOnIsLow);
            digitalWrite(config.ledPinMQTT, config.ledOnIsLow);
            digitalWrite(config.ledPinTCP, config.ledOnIsLow);
            digitalWrite(config.ledPinBLE, config.ledOnIsLow);
        }

        vTaskDelay(500 / MAX_LED_STATES / portTICK_PERIOD_MS);
    }
}

void DashCommsESP::setBLEtimeout(uint16_t timeout) {
    bleButtonTimeoutS = timeout;
    if (timeout > 0) {
        bleCountdown = timeout;
        if (bleCountdown < MIN_BLE_TIMEOUT) {
            bleCountdown = MIN_BLE_TIMEOUT;
        }
        bleCountdown *= 2;
    } else {
        bleCountdown = 0;
    }
}

void DashCommsESP::setLEDsTurnoff(uint16_t timeout) {
    ledsEnabled = true;
    ledsOffTimeoutS = timeout;
    ledsOffCountdown = ledsOffTimeoutS * 2;
}

void DashCommsESP::setBLEpassKey(uint32_t passKey) {
    if (ble_con != nullptr) {
        ble_con->setPassKey(passKey);
    }
} 

void DashCommsESP::startBLE() {
    if (ble_con != nullptr) {
        if (!isBLE) { // Don't restart BLE if already running
            Serial.printf("Starting BLE with %lu second timeout set\r\n", bleCountdown / 2);
            isBLE = true;
            ble_con->begin();
            sendControlMessage(BLE, EN);
        }
    }
}

void DashCommsESP::stopBLE() {
    if (isBLE) {
        isBLE = false;
        if (ble_con != nullptr) {
            ble_con->end();
        }
        sendControlMessage(BLE, HALT);
    }
    bleCountdown = 0;
}

void DashCommsESP::startWiFi(bool allowRestart) {
    if ((wifi != nullptr) && (!isWiFiRunning || allowRestart)) {
        Serial.printf("Starting WiFI: %s %s\n", provisioning->wifiSSID, provisioning->wifiPassword);
        isWiFiRunning = true;
        wifi->begin(provisioning->wifiSSID, provisioning->wifiPassword);
    }
}

void DashCommsESP::stopWiFi() {
    isWiFiRunning = false;
    if (wifi != nullptr) {
        wifi->end();
    }
}

void DashCommsESP::startTCP() {
    if ((wifi != nullptr) && (tcp_con != nullptr)) {
        isTCP = true;
        tcp_con->tcpPort = provisioning->tcpPort;
        wifi->attachConnection(tcp_con);
        startWiFi();
        sendControlMessage(TCP, EN);
    }
}

void DashCommsESP::stopTCP() {
    isTCP = false;
    if (tcp_con != nullptr) {
        tcp_con->end();
    }
    sendControlMessage(TCP, HALT);
    if (wifi != nullptr) {
        wifi->detachTcp();
    }
    if (!isMQTT) {
        stopWiFi();
    }
}

void DashCommsESP::startMQTT() {
    if ((wifi != nullptr) && (mqtt_con != nullptr)) {
        isMQTT = true;
        if (mqtt_con->state != subscribed) {
            mqtt_con->setup(provisioning->dashUserName, provisioning->dashPassword);
            wifi->attachConnection(mqtt_con);
            startWiFi();
        }
        sendControlMessage(MQTT, EN);
    }
}

void DashCommsESP::stopMQTT() {
    isMQTT = false;
    if (mqtt_con != nullptr) {
        mqtt_con->end();
    }
    sendControlMessage(MQTT, HALT);
    if (wifi != nullptr) {
        wifi->detachMqtt(); 
    }
    if (!isTCP) {
        stopWiFi();
    }
}

void DashCommsESP::sleep() {
    Serial.println("Going to sleep");

    if (mqtt_con != nullptr) {
        mqtt_con->sendMessage(dashDevice->getOfflineMessage());
    }
    if (tcp_con != nullptr) {
        tcp_con->end();
    }

    esp_wifi_stop();
    esp_bt_controller_disable();
    esp_deep_sleep_start();
}

void DashCommsESP::run() {
    //handles running connections and monitoring connection timeouts
    if (isWiFiRunning) {
        if (wifi != nullptr) {
            wifi->run();
        }
    }

    if (isBLE) {
        if (ble_con != nullptr) {
            ble_con->run();
        }
    }

    if (moduleMode != MODULE_MODE_DASH_DEVICE) {
        while (config.uart->available() > 0) {
            currentChar = config.uart->read();
            if (currentChar == END_DELIM) {
                serialReceiveBuffer[serialRecieveBufferIndex++] = END_DELIM;
                serialReceiveBuffer[serialRecieveBufferIndex++] = '\0';
                if (serialReceiveBuffer[0] != 0) {
                    memcpy(messageBuffer, serialReceiveBuffer, serialRecieveBufferIndex);
                    parseMessage();
                }
                serialRecieveBufferIndex = 0;
            } else {
                serialReceiveBuffer[serialRecieveBufferIndex++] = currentChar;
            }
        }
    }
}

void DashCommsESP::updateLED(uint8_t pin, uint8_t state) {
    switch (ledTimerCount) {
    case LED_STATE_OFF:
        if (state == LED_STATE_OFF) {
            digitalWrite(pin, config.ledOnIsLow);
        } else {
            digitalWrite(pin, !config.ledOnIsLow);
        }
        break;
    case LED_STATE_STARTUP:
        if (state == LED_STATE_STARTUP) {
            digitalWrite(pin, config.ledOnIsLow);
        }
        break;
    case LED_STATE_SEARCHING:
        if (state == LED_STATE_SEARCHING) {
            digitalWrite(pin, config.ledOnIsLow);
        }
        break;
    }
}
