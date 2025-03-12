#include <DashioCommsESP.h>

const char configC64Str[] PROGMEM =
"rVPLjtowFP2VkddWFcIMVOzIAwZNHjS4oVLVhSGGWAQbOWaAIv6913lQGNpFpcqLXN3HcXzOPWcUOAEafP+B0VsUOxCdUalPBUMD"
"FMVJOAwQRksptJLFxIPkm9OBTMlEFoviFIuEFYyW0K7VnmG0pUc06FgWRjuqmNDVjJd2YUaxLKXFHlr7UNZcV5eQ6otRzvg61wnV"
"XKKB9alrwWm6prLkkBbVL0W+uT6Xh5CL0Fy2okUJF5/arivE82dzMNoIuXBlIRXMh1RJAMIo47Rok2PFmMkdeKbz6/jLM5wXjI4P"
"wLbdt/vdGiOUmXnFKA6CeA4YWw6/aV2AzlngJTWxpGWY+N9IHXmTphYOp03gR1/rKIjHdTBMvTqYBS5p+t2gCbw0nd+p5QxnE9eI"
"pVUBxIxAsxn/CZWODXSvFc/gvfutKNHAtg2FoE+dabUT++21pXOveq2ggXakypj6TR09tYXNWmRtfp5zzZpCm/Oo2jx9HCCKirJa"
"leUJiDO3NsiUi3IhlUSPu2Kmq9iRxwdlb4sP6IaGRB7ggd3uTesNWT2MOLw8otvKAlIwdKkEGwYV3TvFlrysttGweCU/+eATLzA+"
"+W926JlTr9yIF1dOQ54J0/jkGKS/+uXRHFbPHPgtyYW+1bO2AqzCgqkbINePiJ/82Xr/6pu94BoUQHd2cV+TxhtknExf69AhJGqj"
"cWMUdwT2OKOMvfMlmzG93wGUALnwga84zmiZYyfwMXGnOPxCSP0ar2pPOTs0y71aJ+wdwsvlFw==";

#define CONFIG_REV 1

DashCommsESP dashCommsESP("ESP32 Type", "ESP Device Name", configC64Str, CONFIG_REV);
DashDevice *dashDevice;

int dialValue = 0;

void processStatus(ConnectionType connectionType) {
    String message((char *)0);
    message.reserve(1024);

    message = dashDevice->getKnobMessage("KB1", dialValue);
    message += dashDevice->getDialMessage("DL1", dialValue);

    dashCommsESP.sendMessage(message, connectionType);
}

void processIncomingMessage(MessageData * messageData) {
    switch (messageData->control) {
    case status:
        processStatus(messageData->connectionType);
        break;
    case knob:
        if (messageData->idStr == "KB1") {
            dialValue = messageData->payloadStr.toFloat();
            String message = dashDevice->getDialMessage("DL1", dialValue);
            dashCommsESP.sendMessage(message, messageData->connectionType);
        }
        break;
    }
}

void setup() {
    Serial.begin(115200);
    
    dashDevice = dashCommsESP.init(3, 2, true, &processIncomingMessage);
    dashCommsESP.enableRebootAlarm(true);
    dashCommsESP.begin();
}

void loop() {
    dashCommsESP.run();
}    