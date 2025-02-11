#include <dashioCommsESP.h>

void DashCommsESP::parseMessage() { // Parse and act on the contents of the internal messageBuffer
    Serial.print("Incoming->");
    Serial.print(messageBuffer);
    
    char *token = strtok(messageBuffer, DELIM_STR);
    
    // Check for connection prefix in the message
    ConnectionType prefixConnectionType = SERIAL_CONN;
    if (!strncmp(token, BLE, BLELEN)) {
        prefixConnectionType = BLE_CONN;
    } else if (!strncmp(token, TCP, TCPLEN)) {
        prefixConnectionType = TCP_CONN;
    } else if (!strncmp(token, MQTT, MQTTLEN)) {
        prefixConnectionType = MQTT_CONN;
    } else if (!strncmp(token, ALL, ALLLEN)) {
        prefixConnectionType = ALL_CONN;
    }

    if (prefixConnectionType == SERIAL_CONN) { // i.e. connection prefix not detected
        prefixConnectionType = ALL_CONN;
    } else {
        token = strtok(NULL, DELIMETERS_STR);
    }

    // Need to remember to check token isnt NULL before trying to get message length or the esp will crash
    int tokenLength = 0;
    if (token) {
        tokenLength = strlen(token);
    }

    int idLen = dashDevice->deviceID.length() + 1;
    char deviceID[idLen];
    dashDevice->deviceID.toCharArray(deviceID, idLen);

    if (!strncmp(token, CTRL, CTRLLEN)) {
        token = strtok(NULL, DELIMETERS_STR);
        if (token == nullptr) {
            sendControlMessage();
        }
    } else if (!strncmp(token, deviceID, tokenLength)) {
        // If an actual value exists, and its the same as our device name, we can begin parsing in earnest
        token = strtok(NULL, DELIMETERS_STR);
        if ((!strncmp(token, CTRL, CTRLLEN))) {
            // control function
            token = strtok(NULL, DELIMETERS_STR);
            if (token) {
                tokenLength = strlen(token);
            }

            if (!strncmp(token, DEVICE, DEVICELEN)) {
                token = strtok(NULL, DELIMETERS_STR);
                if (token) {
                    tokenLength = strlen(token);
                    dashDevice->type = String(token, tokenLength);

                    token = strtok(NULL, DELIMETERS_STR);
                    if (token) {
                        if (dashDevice->name == DEFAULT_DEVICE_NAME) {
                            tokenLength = strlen(token);
                            dashDevice->name = String(token, tokenLength);
                        }
                    }
                }
            } else if (!strncmp(token, LED, LEDLEN)) {
                token = strtok(NULL, DELIMETERS_STR);
                if (token) { // If there is a token, it should be a timeout value
                    setLEDsTurnoff(atoi(token));
                }
            } else if (!strncmp(token, BLE, BLELEN)) {
                token = strtok(NULL, DELIMETERS_STR);
                bleCountdown = 0;
                if ((!token) || (!strncmp(token, EN, ENLEN))) {
                    startBLE();
                    bleSwEnabled = false;
                } else if (!strncmp(token, HALT, HALTLEN)) {
                    stopBLE();
                    bleSwEnabled = false;
                } else { // If there is a token, and its not a halt, it should be a timeout value
                    setBLEtimeout(atoi(token));
                    bleSwEnabled = true;
                    if (config.buttonPin == GPIO_NUM_NC) {
                        startBLE();
                    }
                }
            } else if (!strncmp(token, WIFI, WIFILEN)) {
                token = strtok(NULL, DELIMETERS_STR);
                if ((!token) || (!strncmp(token, EN, ENLEN))) {
                    startWiFi(true); // Allow restart
                } else if (!strncmp(token, HALT, HALTLEN)) {
                    stopWiFi();
                }
            } else if (!strncmp(token, TCP, TCPLEN)) {
                token = strtok(NULL, DELIMETERS_STR);
                if ((!token) || (!strncmp(token, EN, ENLEN))) {
                    startTCP();
                } else if (!strncmp(token, HALT, HALTLEN)) {
                    stopTCP();
                }
            } else if (!strncmp(token, MQTT, MQTTLEN)){
                //could be a start, or a halt, who knows
                token = strtok(NULL, DELIMETERS_STR);
                if ((!token) || (!strncmp(token, EN, ENLEN))) {
                    startMQTT();
                } else if (!strncmp(token, HALT, HALTLEN)) {
                    stopMQTT();
                }
            } else if (!strncmp(token, REBOOT, REBOOTLEN)) {
                Serial.println("Rebooting");
                ESP.restart();
            } else if (!strncmp(token, SLEEP, SLEEPLEN)) {
                sleep();
            } else if (!strncmp(token, INIT, INITLEN)) {
                serialInitDone = true;
            } else if (!strncmp(token, CNCTN, CNCTNLEN)) {
                // Wants active connections, give all in a tab delineated response
                char respMsg[128] = "\0";
                bool notFirst = false;
                if (isMQTT) {
                    if (notFirst) {
                        strcat(respMsg, DELIM_STR);
                    }
                    notFirst = true;
                    strcat(respMsg, MQTT);
                }

                if (isTCP) {
                    if (notFirst) {
                        strcat(respMsg, DELIM_STR);
                    }
                    notFirst = true;
                    strcat(respMsg, TCP);
                }

                if (isBLE) {
                    if (notFirst) {
                        strcat(respMsg, DELIM_STR);
                    }
                    notFirst = true;
                    strcat(respMsg, BLE);
                }
                strcat(respMsg, "\0");
                sendControlMessage(CNCTN, respMsg); 
                Serial.printf("%s\r\n", respMsg);
            } else if ((!strncmp(token, CFG, CFGLEN))) {
                token = strtok(NULL, DELIMETERS_STR);
                if (token) {
                    tokenLength = strlen(token);
                    token[tokenLength] = '\0';
                    memcpy(configC64, token, tokenLength+1);
                    dashDevice->configC64Str = configC64;
                    Serial.println("Config stored to RAM");

                    token = strtok(NULL, DELIMETERS_STR);
                    if (token) {
                        dashDevice->cfgRevision = atoi(token); // try cast to integer and store as the config revision
                    }
                }
            } else if ((!strncmp(token, STE, STELEN))) {
                token = strtok(NULL, DELIMETERS_STR);
                if (token) {
                    ControlType controlType = dashDevice->getControlType(token);

                    token = strtok(NULL, DELIMETERS_STR);
                    if (token) {
                        if (mqtt_con != nullptr) {
                            mqtt_con->addDashStore(controlType, token);
                        }
                    }
                }
            }
        } else { // Must be a message that requires forwarding, with this deviceID.
            sendNmlMessage(token, deviceID, prefixConnectionType);
        }
    }
}

void DashCommsESP::sendNmlMessage(char *token, char *deviceID, ConnectionType connectionType) {
    serialTransmitBuffer[0] = '\0';

    if (deviceID != nullptr) {
        strcat(serialTransmitBuffer, DELIM_STR);
        strcat(serialTransmitBuffer, deviceID);
    }

    if ((!strncmp(token, CLK, CLKLEN))) { // CLK messages to announce topic
        strcat(serialTransmitBuffer, DELIM_STR);
        strcat(serialTransmitBuffer, CLK);
        int tokenLength = strlen(serialTransmitBuffer);
        serialTransmitBuffer[tokenLength+1] = '\0';
        if (isMQTT) {
            if (mqtt_con != nullptr) {
                mqtt_con->sendMessage(serialTransmitBuffer, announce_topic);
            }
        }
    } else if ((!strncmp(token, ALM, ALMLEN))) { // Alarm messages to alarm topic
        while (token) {
            strcat(serialTransmitBuffer, DELIM_STR);
            strcat(serialTransmitBuffer, token);
            token = strtok(NULL, DELIM_STR);
        }
        int tokenLength = strlen(serialTransmitBuffer);
        serialTransmitBuffer[tokenLength] = '\0';
        if (isMQTT) {
            if (mqtt_con != nullptr) {
                mqtt_con->sendMessage(serialTransmitBuffer, alarm_topic);
            }
        }
    } else { // All other messages to data topic
        while (token) {
            strcat(serialTransmitBuffer, DELIM_STR);
            strcat(serialTransmitBuffer, token);
            token = strtok(NULL, DELIM_STR);
        }
        int tokenLength = strlen(serialTransmitBuffer);
        serialTransmitBuffer[tokenLength+1] = '\0';
        sendMessage(serialTransmitBuffer, connectionType);
    }
}
