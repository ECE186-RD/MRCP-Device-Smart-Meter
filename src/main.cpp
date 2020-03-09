#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <time.h>
//#include <BLENode.h>

//Following code from https://iotbyhvm.ooo/esp32-ble-tutorials/
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;

const int LED = 2; // Could be different depending on the dev board. I used the DOIT ESP32 dev board.
const int mPin1 = 4;  // GPIO 4, input into h-bridge

std::string delimiter = ",";
const double rate = 1/36; // Based off of cost $0.25 per 15 min, but per seconds
time_t start;
double duration;  // double or time_t?
double violationTimeLimit = 300;  // Grace period before violation (seconds)
double sonicSensor = 0;

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
          digitalWrite(LED, HIGH);
          txValue=1;
          double sonicSensor_init = 0; // Read in sensor value now

          while(sonicSensor_init <= sonicSensor) {
            /*
             Will stay in this loop until sensor reads a further distance
             Not closer since someone may walk infront of meter
             Not sure if <= or >=, will change after sensor installed
             Could account for Tape attack
             */
        }

        duration = difftime( time(0), start);  // Stopping the clock
        Serial.println("Stopping!");
        Serial.println(duration);
        double totalCost = round(duration * rate);  // Total cost to customer
        // process payment?
        Serial.println(totalCost);
        digitalWrite(LED, LOW); // Will modify to fit multicolored leds
        txValue=0;
    }
  }
};

void setup() {
  Serial.begin(11500);
  pinMode(LED, OUTPUT);
  pinMode (mPin1, OUTPUT);

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
  if (deviceConnected & (sonicSensor !=0)) {
    // Let's convert the value to a char array:
    char txString[8]; // make sure this is big enough
    dtostrf(txValue, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer

    pCharacteristic->setValue(txString);

    pCharacteristic->notify(); // Send the value to the app!
    Serial.print("*** Sent Value: ");
    Serial.println(txString);
    Serial.print(" ***");
  }
  else if (sonicSensor != 0) {
    while(sonicSensor == 9999){
      // State: s5 = TAPE ALERT
      // If sensor gets covered in tape, then stay here until resolved.
      digitalWrite(LED,HIGH);
    }
      // State: s1 = standby
      start = time(0);     // Restarting the clock
      while(sonicSensor !=0){
        duration = difftime( time(0), start);
        if(duration >= violationTimeLimit){
          // State: s2 = violation
          digitalWrite(LED, HIGH);
        }
      }
  }
  // else, State: s0 = Available
  digitalWrite(LED,LOW);
  delay(100);
}
};
