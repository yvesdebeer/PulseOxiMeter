/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 */
#include <WiFi.h>
#include "BLEDevice.h"
//#include "BLEScan.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MQTT_HOST "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "myoximeter"
#define MQTT_USER "admin"
#define MQTT_TOKEN "admin"
#define MQTT_TOPIC "oximeter/json"

const char* ssid = "<Your-SSID>";    // your network SSID (name)
const char* password = "<SSID-Password>";    // your network password

// MQTT objects
void callback(char* topic, byte* payload, unsigned int length);
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, callback, wifiClient);

// variables to hold data
StaticJsonBuffer<100> jsonBuffer;
JsonObject& payload = jsonBuffer.createObject();
JsonObject& status = payload.createNestedObject("d");
static char msg[50];

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// The remote service we wish to connect to.
static BLEUUID serviceUUID("cdeacb80-5235-4c07-8846-93a37ee6b86d");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("cdeacb81-5235-4c07-8846-93a37ee6b86d");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

int bpm;
int spo2;
float pi;

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Message sent on [");
  Serial.print(topic);
  Serial.println("] ");
}

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    // Serial.println((char*)pData);
    for (int i = 0; i < length; i++) {
      Serial.print(pData[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    if (pData[0] == 0x81) {
      bpm = pData[1];
      spo2 = pData[2];
      pi = pData[3];
    }
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify(notifyCallback);
      Serial.print("Register for Notify Callback");
    }

    connected = true;
    return true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
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
  
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

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

  // Connect to MQTT
  if (mqtt.connect(MQTT_DEVICEID)) {
    Serial.println("MQTT Connected");
    mqtt.subscribe(MQTT_TOPIC);

  } else {
    Serial.println("MQTT Failed to connect!");
    ESP.restart();
  }
} // End of setup.


const uint8_t notificationOff[] = {0x0, 0x0};
const uint8_t notificationOn[] = {0x1, 0x0};
bool onoff = true;

// This is the Arduino main loop function.
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    if (onoff) {
     // Serial.println("Notifications turned on");
      pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
    }
    else     {
     // Serial.println("Notifications turned off");
      pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOff, 2, true);
    }
    onoff = onoff ? 0 : 1;
  }

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
  if (bpm < 255) {
    if (!mqtt.publish(MQTT_TOPIC, msg)) {
      Serial.println("MQTT Publish failed");
    }
  }
  
  delay(3000); // Delay a second between loops.
} // End of loop
