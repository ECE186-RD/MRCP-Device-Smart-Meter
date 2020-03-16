#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <time.h>
//#include <BLENode.h>

int readRange();

//Following code from https://iotbyhvm.ooo/esp32-ble-tutorials/
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;

const int LED = 2; // Could be different depending on the dev board. I used the DOIT ESP32 dev board.

// Pins and variables for Ultrasonic
const int echoPin = 19;   // Red wire
const int trigPin = 21;   // Brown wire
long durationSoundWave;
int distance;
const int maxDetectionDistance = 100; // Distance a car needs to be within
const int minDetectionDistance = 15;   // Too close == error
const int tolerance = 3;              // incorporate to account for error

// Pins for RGB LED
const int ledRed = 18;
const int ledGreen = 17;
const int ledBlue = 16;

std::string delimiter = ",";
const double rate = 1/36; // Based off of cost $0.25 per 15 min, but per seconds
time_t start;
double duration;  // double or time_t?

// 10 seconds is just for testing, ideally 300 seconds?
double violationTimeLimit = 10;  // Grace period before violation (seconds)

double sonicSensor = 1;

//std::string rxValue; // Could also make this a global var to access it in loop()

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received String: ");

        //Parse Data here
        size_t pos = 0;
        if ((pos = rxValue.find(delimiter)) != std::string::npos) {
            std::string holder = rxValue.substr(0, pos);
            std::string user_id = holder;  // This will get the user_id first
            rxValue.erase(0, pos + delimiter.length());
            holder = rxValue.substr(0, pos);
            std::string status = holder; // status = start/stop
        }

        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        } Serial.println(); // Do stuff based on the command received from the app

        if (rxValue.find("START") != -1) {
          // State: s3 = Valid Parking
          start = time(0);     // Restarting the clock
          Serial.println("Starting!");
          txValue=1;
          int sonicSensor_init = readRange(); // Read in sensor value now

          while((sonicSensor_init + tolerance) >= distance) {
            /*
             Will stay in this loop until sensor reads a further distance which means the car has left
             Not closer since someone may walk infront of meter
             Could account for Tape attack
             */
             distance = readRange();
             digitalWrite(ledGreen, HIGH);
             delay(500);
             digitalWrite(ledGreen, LOW);
        }

        duration = difftime( time(0), start);  // Stopping the clock
        Serial.println("Stopping!");
        Serial.println(duration);
        double totalCost = round(duration * rate);  // Total cost to customer
        // process payment?
        Serial.println(totalCost);
        txValue=0;
    }
  }
}
};

void setup() {

  Serial.begin(11500);
  pinMode(LED, OUTPUT);
  pinMode (echoPin, INPUT);
  pinMode (trigPin, OUTPUT);
  //pinMode (ledVolt, OUTPUT); // Leave out for now... Setting LED high
  pinMode (ledRed, OUTPUT);
  pinMode (ledGreen, OUTPUT);
  pinMode (ledBlue, OUTPUT);

  //digitalWrite(ledVolt, HIGH);

  BLEDevice::init("MRCP Smart Meter"); // Give it a name
  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_READ
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
            pAdvertising->addServiceUUID(SERVICE_UUID);
            pAdvertising->setScanResponse(true);
            pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
            pAdvertising->setMinPreferred(0x12);
            BLEDevice::startAdvertising();
  Serial.print("Waiting a client connection to notify...");
}

void loop() {
  distance = readRange();

  if(distance > 40)
    digitalWrite(LED, HIGH);
  else
    digitalWrite(LED, LOW);

  Serial.print("Distance: ");
  Serial.println(distance);

  if (deviceConnected & (distance < maxDetectionDistance)) {
    // Let's convert the value to a char array:
    char txString[8]; // Make sure this is big enough
    dtostrf(txValue, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer

    pCharacteristic->setValue(txString);

    pCharacteristic->notify(); // Send the value to the app!
    Serial.print("*** Sent Value: ");
    Serial.println(txString);
    Serial.print(" ***");
  }
  else if (distance < maxDetectionDistance) {
    while(distance < minDetectionDistance){
      // State: s5 = Maintance Required, LED = Solid Red
      // If sensor gets covered in tape, then stay here until resolved.
      digitalWrite(ledRed, HIGH);
      distance = readRange();
    }
      digitalWrite(ledRed, LOW);

      int initialDistance = distance; // Saving the distance the car is currently at
      start = time(0);     // Restarting the clock
      while(distance <= initialDistance){
        duration = difftime( time(0), start);
        if(duration >= violationTimeLimit){
          // State: s2 = violation, LED = Blink RED
          digitalWrite(ledRed, HIGH);
          delay(250);
          digitalWrite(ledRed, LOW);
          delay(250);
        }
        else{
          // State: s1 = standby, LED = Blink Yellow
          digitalWrite(ledGreen, HIGH);
          digitalWrite(ledBlue, HIGH);
          digitalWrite(ledRed, HIGH);
          delay(500);
          digitalWrite(ledGreen, LOW);
          digitalWrite(ledBlue, LOW);
          digitalWrite(ledRed, LOW);
          delay(500);
        }
        distance = readRange();
      }
  }
  // else, State: s0 = Available, LED = off
  delay(100);
}

int readRange(){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  durationSoundWave = pulseIn(echoPin, HIGH);
  return durationSoundWave*0.034/2; // Returning the distance
}
