/*
 * ECG Health Monitoring System
 * Hardware: ESP32-C3 SuperMini + AD8232 Heart Rate Monitor
 * 
 * Connection Diagram:
 * ESP32-C3 Pin   |   AD8232 Pin   |   Description
 * ---------------------------------------------------
 * 3.3V           |   3.3V         |   Power Supply
 * GND            |   GND          |   Ground
 * GPIO 3 (ADC1_CH3)|  OUTPUT      |   Analog ECG Signal
 * GPIO 10        |   LO+          |   Leads-off Detection (+)
 * GPIO 11        |   LO-          |   Leads-off Detection (-)
 * GPIO 2         |   SDN          |   Shutdown (Optional)
 * 
 * Instructions:
 * 1. Connect electrodes (RA, LA, RL).
 * 2. Open Arduino IDE -> Tools -> Serial Plotter.
 * 3. Set Baud Rate to 115200.
 * 4. Use a BLE app (e.g., nRF Connect) to see live data wirelessly.
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Pin Definitions
const int ECG_OUTPUT_PIN = 3;  // ADC pin
const int LO_PLUS_PIN = 10;    // Leads-off +
const int LO_MINUS_PIN = 11;   // Leads-off -
const int SDN_PIN = 2;         // Shutdown control

// BLE Configuration
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Custom UUIDs for ECG Service
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  // Initialize Serial for Plotting
  Serial.begin(115200);

  // Configure Pins
  pinMode(LO_PLUS_PIN, INPUT);
  pinMode(LO_MINUS_PIN, INPUT);
  pinMode(SDN_PIN, OUTPUT);
  digitalWrite(SDN_PIN, HIGH); // Enable AD8232

  // Configure ADC (ESP32-C3 ADC is 12-bit by default)
  analogReadResolution(12);

  // Create the BLE Device
  BLEDevice::init("ESP32-C3-ECG-Band");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("BLE Ready! Waiting for a client connection...");
}

void loop() {
  // 1. Check Leads-off status
  bool leadsOff = (digitalRead(LO_PLUS_PIN) == HIGH) || (digitalRead(LO_MINUS_PIN) == HIGH);

  int ecgValue = 0;

  if (leadsOff) {
    // If electrodes are disconnected, output 0 or flatline
    ecgValue = 0;
    Serial.println("Leads-Off! Check electrode connections.");
  } else {
    // 2. Read Analog Signal
    ecgValue = analogRead(ECG_OUTPUT_PIN);
    
    // 3. Print to Serial Plotter
    // Format: "Min:0,Max:4095,Value:ecgValue" (Helps scale the plotter)
    Serial.print("0,4095,"); 
    Serial.println(ecgValue);
  }

  // 4. Send via BLE if connected
  if (deviceConnected) {
    String valueStr = String(ecgValue);
    pCharacteristic->setValue(valueStr.c_str());
    pCharacteristic->notify();
    // delay(10); // Adjust based on app requirements
  }

  // Handle BLE reconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("Restarting Advertising...");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff on connecting
    oldDeviceConnected = deviceConnected;
  }

  // Control Sampling Rate (approx 100Hz - 200Hz is common for basic ECG)
  delay(10); 
}
