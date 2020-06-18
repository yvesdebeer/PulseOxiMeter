Oximeter sensor with Arduino

###1. Load example Sketch 

File - Examples - ESP32 BLE Arduino - BLE_client
	
###Replace the BLE serviceUUID as well as the BLE charUUID:
	
```
// The remote service we wish to connect to.
static BLEUUID serviceUUID("cdeacb80-5235-4c07-8846-93a37ee6b86d");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("cdeacb81-5235-4c07-8846-93a37ee6b86d");
```
	
###Add logging to notifyCallback(...)
	
```
for (int i = 0; i < length; i++) {
  Serial.print(pData[i], HEX);
  Serial.print(" ");
}
```

###Add 3 variables   
	
```
int bpm;
int spo2;
float pi;
```
	
###Assign values to those 3 variables
	
```
Serial.println();
if (pData[0] == 0x81) {
  bpm = pData[1];
  spo2 = pData[2];
  pi = pData[3];
}
```
    
###Connect external display
    
Add 2 libs:
    
```
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
```
	
```
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
```

Add code to Setup()

```
// SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
Serial.println(F("SSD1306 allocation failed"));
for (;;); // Don't proceed, loop forever
}
// Show initial display buffer contents on the screen --
// the library initializes this with an Adafruit splash screen.
display.display();
delay(200); // Pause for 2 seconds
display.clearDisplay();
display.display();
```	
Add code to Loop()

```
display.clearDisplay();
display.setTextSize(2);
display.setTextColor(WHITE);
display.setCursor(0, 10);
display.println("BPM");
display.setCursor(60, 10);
display.println(bpm);
display.setCursor(0, 30);
display.println("SPO2:");
display.setCursor(60, 30);
display.println(spo2);
display.setCursor(0, 50);
display.println("PI:");
display.setCursor(60, 50);
display.println(pi/10.0);
display.display();
```	

###Connect to WiFi

```
#include <WiFi.h>
#include "credentials.h"
```
```
const char* ssid = mySSID;
const char* password = myPASSWORD;
```	
credentials.h (create file in Arduino/Library Folder)

```
#define mySSID "<SSID>"
#define myPASSWORD "<WiFiPASSWORD>"
```

Add code to setup()

```
// Start WiFi connection
Serial.print("Connecting to ");
Serial.println(ssid);

// Start WiFi connection
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
  
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}

Serial.println("");
Serial.println("WiFi connected");  
Serial.print("IP address: ");
Serial.println(WiFi.localIP());
```

###Add code for MQTT

Add PubSubClient by Nick O'Leary library

```
#include <ArduinoJson.h>
#include <PubSubClient.h>
```

```
#define MQTT_HOST "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "myoximeter"
#define MQTT_USER "admin"
#define MQTT_TOKEN "admin"
#define MQTT_TOPIC "oximeter/json"
```

Add definitions

```
// MQTT objects
void callback(char* topic, byte* payload, unsigned int length) {
}
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, callback, wifiClient);

// variables to hold data
StaticJsonBuffer<100> jsonBuffer;
JsonObject& payload = jsonBuffer.createObject();
JsonObject& status = payload.createNestedObject("d");
static char msg[50];
```

Add code to setup()

```
// Connect to MQTT
if (mqtt.connect(MQTT_DEVICEID)) {
  Serial.println("MQTT Connected");
  mqtt.subscribe(MQTT_TOPIC);

} else {
  Serial.println("MQTT Failed to connect!");
  ESP.restart();
}
```

Add code to loop()

```
mqtt.loop();
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(MQTT_DEVICEID)) {
      Serial.println("MQTT Connected");
      //mqtt.subscribe(MQTT_TOPIC_CMD);
      mqtt.loop();
    } else {
      Serial.println("MQTT Failed to connect!");
      delay(5000);
    }
  }

  // Send data to IBM Cloud Platform
  status["bpm"] = bpm;
  status["spo2"] = spo2;
  status["pi"] = pi/10;
  payload.printTo(msg, 50);
  Serial.println(msg);
  if (bpm > 0) {
    if (!mqtt.publish(MQTT_TOPIC, msg)) {
      Serial.println("MQTT Publish failed");
    }
  }
 ```


