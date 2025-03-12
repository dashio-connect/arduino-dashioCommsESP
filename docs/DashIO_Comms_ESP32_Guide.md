<h1 id="toc_0">Dash IoT Comms Guide (ESP32)</h1>

**12 March 2024**

This guide demonstrates how to make BLE, TCP and MQTT connections on an ESP32 IoT device using the Arduino IDE and the **DashIO** Adruino library. It also shows how to load user controls (widgets) into your mobile device to monitor and control an IoT device.

This guide is based on the **DashioCommsESP** library, which is a wrapper for the **DashioESP** library. It simplifies setting up connections and provisioning for ESP32 develoment.

This library is best to use were you need multiple connection types are required to be used (e.g. BLE and MQTT) because it simplifies the setup.

<h2 id="toc_1">Getting Started</h2>

For the big picture on **Dash**, take a look at our website: <a href="https://dashio.io">dashio.io</a>

For the **DashioCommsESP** arduino library: <a href="https://github.com/dashio-connect/arduino-dashioCommsESP">https://github.com/dashio-connect/arduino-dashioCommsESP</a>

<h2 id="toc_2">Requirements</h2>

Grab an ESP32 board, Arduino IDE and follow this guide. The examples in this guide should work on ESP32 development boards available for the Arduino IDE.

<img src="https://dashio.io/wp-content/uploads/2022/09/attention_44_yellow.png" width="20"> **Attention:** Make sure you use an ESP32 that has both BLE and WiFi capabilities.

You will need to install the **Dash** app on your mobile phone or tablet from the <a href="https://apps.apple.com/us/app/dash-iot/id1574116689">App Store</a> or <a href="https://play.google.com/store/apps/details?id=com.dashio.dashiodashboard">Google Play</a>. Search for **Dash IoT**.

If you'd like to connect to your IoT devices through the **Dash** MQTT broker, setup an account on the <a href="https://dashio.io">dashio.io</a> website.

<h2 id="toc_3">Install</h2>

<h3 id="toc_4">Arduino IDE</h3>

If you haven't yet installed the ESP32 Arduino IDE support from espressif, please check the following link for instructions: <a href="https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html">https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html</a>

You will need to add the **DashioCommsESP** library into your project. It is included in the Arduino IDE Library manager. Search the library manager for the library titled "DashioCommsESP" and install. The Arduino IDE may also ask you to install the following libraries (dependencies). Please make sure they are all installed from the Arduino IDE Library Manager.

- **DashioESP** is the core ESP32 library used for ESP32 connections.
- **Dashio** is the core messageing library used to manage messages.
- **NimBLE-Arduino** by h2zero is required for BLE messaging.
- **MQTT** by Joël Gähwiler is required for MQTT messaging.
- **Preferences** by Volodymyr Shymanskyy is used for storing WiFi and login credentials etc.

<h2 id="toc_5">Guide</h2>

<h3 id="toc_6">Basics</h3>

Lets start with setting up connections.

```
#include <DashioCommsESP.h>

DashCommsESP dashCommsESP("ESP32 Type", "ESP Device Name");
DashDevice *dashDevice;

void setup() {
    Serial.begin(115200);
    
    dashDevice = dashCommsESP.init(3, 2, true);
    dashCommsESP.begin();
}

void loop() {
    dashCommsESP.run();
}
```

This is about the fewest number of lines of code necessary to get talking to the <strong>Dash</strong> app. There is a lot happening under the hood to make this work. After the <code>#include "DashioCommsESP.h"</code> we create a DashioCommsESP instance with the *device_type* and *device_name* as its attributes. This will create a dash device with its device_ID set to the mac address of the ESP32 WiFi peripheral.

In the ```setup()``` function we initialise the dashCommsESP with ```dashDevice = dashCommsESP.init(num_ble_cnctns, num_tcp_cnctns, use_dash_mqtt);```. The three attributes setup the required connections, which in this case are:

- **num\_ble\_cnctns** = 3 BLE connections
- **num\_tcp\_cnctns** = 2 TCP connections (on port 5650)
- **use\_dash\_mqtt** = Dash MQTT is enabled

Notice that ```dashCommsESP.init``` returns a pointer to a DashDevice object. We will use this later on.

In the ```setup()``` function we also start the connections with ```dashCommsESP.begin();```.

The impoertant final step it to run the dashDevice in the ```loop()``` function by calling ```dashCommsESP.run();```

This device is discoverable by the **Dash** app.

You can also discover your IoT device using a third party BLE scanner (e.g. "BlueCap" or "nRF Connect") The BLE advertised name of your IoT device will be a concatentation of "DashIO_" and the <em>device_type</em>.

Setup the Arduino IDE serial monitor to 115200 baud and run the above code. Then run the **Dash** app on your mobile device and you will see connection "WHO" messages on the Arduino serial monitor as the Dash app detects and communicates with your IoT device via the BLE connection.

TCP and Dash MQTT connections won't be available as we haven't setup their credentials. We will get to that later when we look a **provisioning**.

<img src="https://dashio.io/wp-content/uploads/2022/09/attention_44_yellow.png" width="20"> <strong>Troubleshooting:</strong>

- Occasionally, the <strong>Dash</strong> app is unable to discover a BLE connection to the IoT device. If this occurs, try deleting the the IoT device from the Bluetooth Settings of your phone or tablet.
- Set the **Core Debug Level** in the IDE to "Info" so you can see what is happening.
- More capable ESP32 microcontrollers (e.g. ESP32-S3) can manage more connections as there is less conflicts of resources which can lead to crashes.
- If you have been usimg the ESP32 for another project, it is advisable to set "Erase ALL Flash Before Sketch Upload" to "Enabled" in the IDE for your first upload, then you can set it back to "Disabled" afterwards. This can avoid spurious data in the NV data storage from potentially causing issues.
- Make sure you set the partition scheme to allow enough for code space, because running BLE and WiFi together consumes a big chunk of cod space.


<h3 id="toc_7">Controls on the Dash IoT App</h3>

Lets add Dial control messages that are sent to the **Dash** app every second. To do this we create a new task to provide a 1 second time tick and then send a Dial value message from the loop every second.

```
#include <DashioCommsESP.h>

DashCommsESP dashCommsESP("ESP32 Type", "ESP Device Name");
DashDevice *dashDevice;

bool oneSecond = false; // Set by timer every second.

void oneSecondTimerTask(void *parameters) {
    for(;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        oneSecond = true;
    }
}

void setup() {
    Serial.begin(115200);
    
    dashDevice = dashCommsESP.init(3, 2, true);
    dashCommsESP.begin();

    // Setup 1 second timer task
    xTaskCreate(
        oneSecondTimerTask,
        "One Second",
        1024, // stack size
        NULL,
        1, // priority
        NULL // task handle
    );
}

void loop() {
    dashCommsESP.run();

    if (oneSecond) { // Tasks to occur every second
        oneSecond = false;
        dashCommsESP.sendMessageAll(dashDevice->getDialMessage("DL1", int(random(0, 100))));
    }
}
```

The line ```dashCommsESP.sendMessageAll(dashDevice->getDialMessage("DL1", int(random(0, 100))));``` creates and sends a message with two parameters to the **Dash** app. The first parameter is the *control_ID* which identifies the specific Dial control in the **Dash** app and the second parameter is simply the Dial value, a random number in this case. It sends this message through ALL connections. Notice that the ```dashDevice``` pointer is used to create the message, in this case the dial message.

Once again, run the above code and the **Dash** app. This time, "DIAL" messages will be seen on the serial monitor every second.

The next step is to show the Dial values from the messages on a control on the **Dash** app.

<h3 id="toc_8">Adding Controls to Dash App</h3>

In the **Dash** app, tap the <img src="https://dashio.io/wp-content/uploads/2021/07/iot_blue_44.png" width="20"> <strong>All Devices</strong> button, followed by the <img src="https://dashio.io/wp-content/uploads/2021/07/magnifying_glass_44_blue.png" width="20"> <strong>Find New Device</strong> button. Then select the <strong>BLE Scan</strong> option to show a list of new IoT devices that are BLE enabled. Your IoT device should be shown in the list. Select your device and from the next menu select <strong>Create Device View</strong>. This will create an empty **Device View** for your new IoT deivce. You can now add controls to the Device View:

Once you have discovered your IoT device in the **Dash** app and have a **Device View** available for editing, you can add controls to the **Device View**:

<ul>
<li><strong>Start Editing</strong>: Tap the <img src="https://dashio.io/wp-content/uploads/2021/07/pencil_44_blue.png" width="20"> <strong>Edit</strong> button (if not already in editing mode).</li>
<li><strong>Add Dial Control</strong>: Tap the <img src="https://dashio.io/wp-content/uploads/2021/07/add_44_blue.png" width="20"> <strong>Add Control</strong> button and select the Dial control from the list.</li>
<li><strong>Edit Controls</strong>: Tap the Dial control to select it. The <img src="https://dashio.io/wp-content/uploads/2021/07/spanner_44_button.png" width="20"> <strong>Control Settings Menu</strong> button will appear in the middle of the Control. The Control can then be dragged and resized (pinch). Tapping the <img src="https://dashio.io/wp-content/uploads/2021/07/spanner_44_button.png" width="20"> button allows you to edit the Control settings where you can setup the style, colors and other characteristics of your Control. Make sure the <em>Control_ID</em> is set to the same value that is used in the Dial messages (in this case it should be set to "DL1").</li>
<li><strong>Quit editing</strong>: Tap the <img src="https://dashio.io/wp-content/uploads/2021/07/pencil_quit_44.png" width="20"> <strong>Edit</strong> button again.</li>
</ul>

The Dial on the **Dash** app will now show the random Dial values as they arrive.

The next piece of the puzzle to consider is how your IoT device can receive data from the **Dash** app. Lets add a Knob and connect it to the Dial.

In the **Dash** app you will need to add a **Knob** control onto your **Device View**, next to your **Dial** control. Edit the **Knob** to make sure the **Control ID** of the Knob matches what you have used in your Knob messages (in this case it should be "KB1"), then quit edit mode.

In the Arduino code we need to respond to messages coming in from a Knob control that we just added to the **Dash** app. To make the changes to your IoT device we add a callback, **processIncomingMessage**, into the dashCommsESP init function to become ```dashCommsESP.init(3, 2, true, &processIncomingMessage);```

```
#include <DashioCommsESP.h>

DashCommsESP dashCommsESP("ESP32 Type", "ESP Device Name");
DashDevice *dashDevice;

void processIncomingMessage(MessageData * messageData) {
    switch (messageData->control) {
    case knob:
        if (messageData->idStr == "KB1") {
            String message = dashDevice->getDialMessage("DL1", (int)messageData->payloadStr.toFloat());
            dashCommsESP.sendMessage(message, messageData->connectionType);
        }
        break;
    }
}

void setup() {
    Serial.begin(115200);
    
    dashDevice = dashCommsESP.init(3, 2, true, &processIncomingMessage);
    dashCommsESP.begin();
}

void loop() {
    dashCommsESP.run();
}
```

Remember to remove the timer and the associated Dial code from the loop() function.

We obtain the Knob value from the message data payload that we receive in the MessageData of the ```processIncomingMessage``` function. We then create a Dial message with the value from the Knob and send this back to the **Dash** app.

When you adjust the Knob on the **Dash** app, a message with the Knob value is sent your IoT device, which returns the Knob value into the Dial control, which you will see on the **Dash** app.

The MessageData contains a lot of useful information, including the ConnectionType the incoming message arrived on. In this example we reply to an incoming message only on the same connection that it arrived on, by calling  ```sendMessage(message, messageData->connectionType);```, where the ConnectionType is obtained from the messageData.

You can choose how you want to reply, and if you want to reply to all connections, use ```dashCommsESP.sendMessageAll(message);```

<h3 id="toc_9">Responding to STATUS Messages</h3>

STATUS messages allows the IoT device to send initial conditions for each control to the **Dash** app as soon as a connection becomes active. Therefore, it is important to respond to the STATUS message. Once again, we do this from the ```processIncomingMessage``` function and our complete code looks like this:

```
#include <DashioCommsESP.h>

DashCommsESP dashCommsESP("ESP32 Type", "ESP Device Name");
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
    dashCommsESP.begin();
}

void loop() {
    dashCommsESP.run();
}
```

If you would like to use a secure (encrypted and bonded) BLE connection, simply add a 6 digit numeric passkey (greater than 0) before ```dashCommsESP.begin();``` with the following function:

```
dashCommsESP.setBLEpassKey(654321);
```

The mobile device will then require you to enter the passkey when it first connects with BLE to the IoT device. Secure BLE connections on the ESP32 require significant resources and may not run simultaniously with other connections. For combining connections (e.g. BLE and MQTT), you should ideally include a push button on your IoT device to enable the user to enable the BLE connection and the passkey should then be displayed on the IoT device for the user to enter on their mobile device.

<h2 id="toc_10">Layout Configuration</h2>

**Layout configuration** allows the IoT device to hold a copy of the complete layout of the device as it should appear on the **Dash** app. It includes all infomration for the device, controls (size, colour, style etc.), device views, connections and provisioning.

When the **Dash** app discovers a new IoT device, it will download the Layout Configuration from the IoT device to display the device controls they way they have been designed by the developer. This is particularly useful when developing commercial products and distributing your IoT devices to other users.

To include the Layout Configuration in your IoT device, simply follow these steps:

<ul>
<li><strong>Design your layout</strong> in the <strong>Dash</strong> app and include all controls and connections that you need in your layout.</li>
<li><strong>Export the layout</strong>: Tap on the <strong>Device</strong> button, then tap the <strong>Export Layout</strong> button.</li>
<li><strong>Select the connections and provisioning setup</strong> that you want (see below for provisioning details) and tap the <strong>Email Config</strong> button and the Layout Configuration will be emailed to you. Alternatively, tap the <strong>View Config</strong> button and you can copy and paste the layout directly from the <strong>Dash</strong> app to share whatever way you like.</li>
<li><strong>Copy and paste</strong> the C64 configuration text from the email into your Arduino code, assigning it to a pointer to store the text in program memory. Your C64 configuration text will be different to that shown below.</li>
<li><strong>Add the pointer</strong> to the C64 configuration text (configC64Str) as a third attribute to the DashCommsESP object.</li>
<li><strong>Add the Layout Config Revision</strong> integer (CONFIG_REV) as a fourth attribute to the DashDevice object.</li>
</ul>

```
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
```

Here is our Knob and Dial example, with BLE, TCP and MQTT connections and Layout Configuration added:

```
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
    dashCommsESP.begin();
}

void loop() {
    dashCommsESP.run();
}
```

Make sure you download the new layout drom your IoT device into your **Dash** app. In the **Dash** app, tap the <img src="https://dashio.io/wp-content/uploads/2021/07/iot_blue_44.png" width="20"> **Device** button for your IoT device. The **Device** menu will be shown. Then tap the **Get Layout** button in the footer of the menu to download the new layout and provisioning setup.

<h2 id="toc_11">Provisioning (Commissioning)</h2>

Provisioning is the method by which the **Dash** app can send user credentials and other setup information to the IoT device and the IoT device will store this data in non-volatile memory.

The provisioning features of the DashCommsESP library manages and stores the following provisioning data in non-volatile memory:

- Device Name
- WiFi SSID
- WiFI Password
- Dash MQTT broker User Name
- Dash MQTT broker Password

<img src="https://dashio.io/wp-content/uploads/2022/09/attention_44_yellow.png" width="20"> **Attention:** Details of the provisioning functionality available in an IoT device are contained in the **Layout Configuration** and must be setup when exporting the Layout Configuration from the **Dash** app.

Your IoT device will let the **Dash** app know what provisioning services are available when it downloads the **layout configuration**.

<h3 id="toc_12">TCP & Dash MQTT Provisioning</h3>

You can now provision your TCP and Dash MQTT connections that you have setup in the previous examples. In the **Dash** app, tap the <img src="https://dashio.io/wp-content/uploads/2021/07/iot_blue_44.png" width="20"> **Device** button for your IoT device and the tap <img src="https://dashio.io/wp-content/uploads/2022/09/key_44_blue.png" width="20"> **Provisioning** menu to setup your WiFi and **Dash** account credentials.

This device's TCP connection is discoverable by the **Dash** app. You can also discover your IoT device using a third party Bonjour/Zeroconf discovery tool. The mDNS service will be "\_DashIO.\_tcp." and individual IoT devices on this service are identified by their mac address (<em>device_ID</em>).

Run the **Dash** app and tap the <img src="https://dashio.io/wp-content/uploads/2021/07/iot_blue_44.png" width="20"> **All Devices** button, followed by the <img src="https://dashio.io/wp-content/uploads/2021/07/magnifying_glass_44_blue.png" width="20"> **Find New Device** button. Then select the **TCP Discovery** option to show a list of new IoT devices that are mDNS enabled. Your device will be shown. Select your IoT device.

Run the **Dash** app once more. Tap the <img src="https://dashio.io/wp-content/uploads/2021/07/iot_blue_44.png" width="20"> **All Devices** button, followed by the <img src="https://dashio.io/wp-content/uploads/2021/07/magnifying_glass_44_blue.png" width="20"> **Find New Devices** button. Then select the **My Devices On Dash** option to show a list of new IoT devices that have announced themselves to the **dash** server. Your device will be shown. Select your IoT device to add the MQTT connection.

<h2 id="toc_13">Dash MQTT Features</h2>

The **dash** server have various features thay are available through the MQTT connection to give your IoT devices important capabilities.

<h3 id="toc_14">Alarms</h3>

For MQTT connections operating through the **dash** MQTT broker, a service is available for sending alarms (push notifications) to your mobile device. These are handy for sending messages or alarms to the user when they are not currently using the **Dash** app. To send an alarm, simply call the ```sendAlarm``` method with any an alard identifier, alarm title and alarm message to be displayed as a notification on the phone or tablet, for example:

```
dashCommsESP.sendAlarm("AL1", "Alarm Title", "Alarm Message");
```
The alarm identifier (e.g. "AL1"above) allows the **Dash** app to setup a specific notification sound for all alarms with the same alarm identifier.

You can also instruct dashCommsESP to send an alarm every time the ESP32 reboots. This is very useful for debugging the long term stability of you code. To enable or disable the reboot alarm, call the folowing function:

```
dashCommsESP.enableRebootAlarm(true);
```

Note that the reboot alarm will not operate if the ESP32 is waking from sleep mode.

<h3 id="toc_15">Data Storage Setup</h3>

The **dash** server stored data for controls such as **TimeGraphs**, **EventLogs** and **Maps**. Although these data store can be setup on eth **Dash** app, it is preferable for the IoT device to let the **dash** server know what data store it required. This is done with the ```addDashStore(controlType, controlID)``` function call with the *controlType* and *controlID* as attributes. The following sets up the **dash** store for a TimeGraph control:

```
dashCommsESP.addDashStore(timeGraph, "T01");
```

Please note that ```addDashStore``` must be called before ```dashCommsESP.begin()```;

<h3 id="toc_16">Hardware Configuration</h3>

The **DashCommsESP** class can be configured to interact with various GPIO of the ESP32. The configuration data in stored in the class **DashCommsConfig** and is accessed via ```dashCommsESP.config``` as sdescribed in the following sections.

<h4 id="toc_17">Connection Status LEDs</h4>

The **DashCommsESP** class has the capability of managing up to 4 LEDs for showing the connection state for WiFi, MQTT, TCP and BLE.

To enable the LEDs, simply set their pins in the dashCommsESP.config struct as shown in the following table:

| Config Pin Name | Description | Type | Default |
|----|----|----|----|
| ledPinWiFi | WiFi connection LED pin | gpio\_num\_t | GPIO\_NUM\_NC |
| ledPinMQTT | MQTT connection LED pin | gpio\_num\_t | GPIO\_NUM\_NC |
| ledPinTCP | TCP connection LED pin | gpio\_num\_t | GPIO\_NUM\_NC |
| ledPinBLE | BLW connection LED pin | gpio\_num\_t | GPIO\_NUM\_NC |
| ledOnIsLow | LEDs are active low (pulldown) | bool | true |
| enableLEDtest | Enable powerup test of LEDs | bool | true |

For example, set the BLE LED pin to pin 4 as follows:

```
dashCommsESP.config.ledPinBLE = GPIO_NUM_4;
```

Adjusting values in dashCommsESP.config must be done before calling the function ```dashCommsESP.init```.

You can also set a timer to turn off the LEDs after a period of time using the following command:

```
dashCommsESP.setLEDsTurnoff(timeout);
```

The **timeout** is in seconds and if set to 0, the timeout is disabled.


<h4 id="toc_18">BLE Enable/Disable Button & Timeout</h4>

The **DashCommsESP** class has the capability of managing a single push putton switch to enable and disable the BLE connections. By default, the button is disabled. You can assign a pin in the dashCommsESP.config struct for the button. For example, set the BLE button to pin 6 as follows:

```
dashCommsESP.config.buttonPin = GPIO_NUM_6;
```

Adjusting values in dashCommsESP.config must be done before calling the function ```dashCommsESP.init```.

Note that the button should be wired as active low.

You can also set a timer to disable the BLE connections after a period of time of BLE message inactivity using the following command:

```
dashCommsESP.setBLEtimeout(timeout);
```

The **timeout** is in seconds and if set to 0, the timeout is disabled.


<h4 id="toc_19">Wakeup</h4>

Should you wish to put the ESP32 into sleep mode, a wakeup pin can be assigned in the dashCommsESP.config, for example:

```
dashCommsESP.config.extWakeupPin = GPIO_NUM_33;
```


<h1 id="toc_20">Jump In and Build Your Own IoT Device</h1>

When you are ready to create your own IoT device, the Dash Arduino C++ Library will provide you with more details about what you need to know:

<a href="https://dashio.io/arduino-library/">https://dashio.io/arduino-library/</a>
