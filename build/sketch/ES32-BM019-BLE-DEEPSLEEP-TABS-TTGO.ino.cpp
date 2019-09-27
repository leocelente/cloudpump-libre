#include <Arduino.h>
#line 1 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO.ino"
#line 1 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO.ino"
#include <time.h>
#define NFC_FAILED_COMM -1
#define NFC_FAILED_CONF 0
#define NFC_FAILED_SCAN 1
#define NFC_SUCCESS 2
#define BLE_SYNC 3

#define uS_TO_S_FACTOR                                                         \
  1000000               /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 5 /* Time ESP32 will go to sleep (in seconds) */

enum class State {
  StabilishComm,
  SetConfiguration,
  Scan,
  ScanFinished,
  Sync,
  StabilishCommError,
  SetConfigurationError,
  ScanningError,
  SyncError
};

State state = State::StabilishComm; // used to track NFC state

#line 26 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO.ino"
void setup();
#line 41 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO.ino"
void loop();
#line 81 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ble.ino"
void startBLE();
#line 112 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ble.ino"
void ble_save(int value);
#line 139 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ble.ino"
long getTime();
#line 146 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ble.ino"
uint8_t ble_convert(float gluc);
#line 29 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
void sendCommand(const byte cmds[], const int size);
#line 39 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
void waitForResponse();
#line 52 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
void spiRead(byte *readError);
#line 65 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
float Glucose_Reading(unsigned int val);
#line 69 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
void nfc_comm();
#line 110 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
void nfc_conf();
#line 138 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
void nfc_scan();
#line 163 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
float nfc_get_glucose();
#line 400 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
void nfc_startup();
#line 4 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nvs.ino"
void startNVS();
#line 1 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\pwr.ino"
void nfc_sleep();
#line 26 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO.ino"
void setup() {
  Serial.begin(115200);
  Serial.println("Starting....");

  //  startNVS();

  nfc_startup();
  startBLE();
  //  routine();
  //  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //  Serial.println("Starting deep sleep...");

  //  esp_deep_sleep_start();
}

void loop() {
  if (state == State::StabilishComm)
    nfc_comm();
  else if (state == State::SetConfiguration)
    nfc_conf();
  else if (state == State::Scan)
    nfc_scan();
  else if (state == State::ScanFinished) {
    float fGlucose = nfc_get_glucose();

    Serial.println("\nfGlucose Reading: " + String(fGlucose));

    int nGlucose = static_cast<int>(fGlucose);
    ble_save(nGlucose);

    nfc_sleep();
  }
  else state = State::StabilishComm;
  //  else if (NFCReady == BLE_SYNC) ble_sync();
  delay(1000);
}

#line 1 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\ble.ino"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
// #include <string>

#define BLE_NAME "CGM32"

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
BLECharacteristic *rx = NULL;
BLECharacteristic *tx = NULL;
BLECharacteristic *ad = NULL;

// Array that contains glucose readings
int readings[24];
int current_index = 0;

// Keeps track of connection state
bool deviceConnected = false;
bool oldDeviceConnected = false;

class ConnectionCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *_server) {
    Serial.println("Device Connected!");
    deviceConnected = true;
  }
  void onDisconnect(BLEServer *_server) {
    Serial.println("Device Disconnected!");
    deviceConnected = false;
  }
};

class AdCallbacks : public BLECharacteristicCallbacks{
  void onWrite(BLECharacteristic *_ad){}
  void onRead(BLECharacteristic *_ad){}
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *_characteristic) {
    Serial.println("RX being written to");
    std::string rxValue = _characteristic->getValue();
    // Serial.println(rxValue);
    if (rxValue.length() > 0) {
      if (rxValue.find("W") != -1) {
        Serial.println("Command Received: w");
        ble_save((current_index + 1) * 40);
      } else if (rxValue.find("P") != -1) {
        Serial.println("Command Received: p");
        int i;
        for (i = 0; i < sizeof(readings) / sizeof(int); i++)
          Serial.println(String(readings[i]).c_str());
      }
    }
  }
};

class TxCallbacks : public BLECharacteristicCallbacks {
  int c = 0;
  void onRead(BLECharacteristic *_tx) {
    Serial.print("Reading number: ");
    Serial.println(c);
    String t = String(getTime());
    String value = t + String(readings[c]);
    _tx->setValue(value.c_str());
    Serial.print("Value set to:");
    Serial.println(value);
    if (readings[c] == 0) {
      current_index = 0;
      c = 0;
    } else {
      c++;
    }
  }
};

void startBLE() {
  BLEDevice::init(BLE_NAME);
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ConnectionCallbacks());
  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  rx = pService->createCharacteristic(CHARACTERISTIC_UUID_RX,
                                     BLECharacteristic::PROPERTY_WRITE_NR);

  tx = pService->createCharacteristic(CHARACTERISTIC_UUID_TX,
                                     BLECharacteristic::PROPERTY_READ);

  ad = pService->createCharacteristic(CHARACTERISTIC_UUID_TX,
                                      BLECharacteristic::PROPERTY_READ);

  rx->addDescriptor(new BLE2902());
  tx->addDescriptor(new BLE2902());
  tx->addDescriptor(new BLE2902());

  rx->setCallbacks(new RxCallbacks());
  tx->setCallbacks(new TxCallbacks());
  ad->setCallbacks(new AdCallbacks());
  

  pService->start();
  pServer->getAdvertising()->start();
}

void ble_save(int value) {
  Serial.print("current_index: ");
  Serial.println(current_index);
  Serial.print("Saving new reading: ");
  Serial.println(value);

  readings[current_index] = value;
  current_index++;

  ad->setValue(String(value).c_str());

  Serial.print("new current_index: ");
  Serial.println(current_index);

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}

long getTime() {
  if (1540508787 > INT_MAX)
    Serial.println("TOO SMALL");
  //  ADD RTC
  return 1540508787;
}

uint8_t ble_convert(float gluc) {
  int iValue = static_cast<int>(gluc);
  return (uint8_t)iValue;
}

#line 1 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nfc.ino"
/*
  NFC Communication with the Solutions Cubed, LLC BM019
  and an Arduino Uno.  The BM019 is a module that
  carries ST Micro's CR95HF, a serial to NFC converter.

  Wiring:
  Arduino          BM019
  IRQ: Pin 9       DIN: pin 2
  SS: pin 10       SS: pin 3
  MOSI: pin 11     MOSI: pin 5
  MISO: pin 12     MISO: pin4
  SCK: pin 13      SCK: pin 6

*/
#include <SPI.h>

#define MAX_NFC_READTRIES 10 // Amount of tries for every nfc block-scan

// Delay between steps
#define CMD_DELAY 100

#define SS_PIN 5
#define IRQ_PIN 26

// byte TXBuffer[40];    // transmit buffer (instead SPI.h is used)
byte RXBuffer[40]; // receive buffer

/// Sends command through SPI
void sendCommand(const byte cmds[], const int size) {
  digitalWrite(SS_PIN, LOW);
  for (int i = 0; i < size; i++) {
    SPI.transfer(cmds[i]);
  }
  digitalWrite(SS_PIN, HIGH);
  delay(1);
}

/// Waits for command response
void waitForResponse() {
  digitalWrite(SS_PIN, LOW);
  while (RXBuffer[0] != 8) {
    RXBuffer[0] = SPI.transfer(0x03); // Write 3 until
    RXBuffer[0] = RXBuffer[0] & 0x08; // bit 3 is set
  }
  digitalWrite(SS_PIN, HIGH);
  delay(1);
}



// Other read used to fill the buffer
void spiRead(byte *readError) {
  digitalWrite(SS_PIN, LOW);
  SPI.transfer(0x02);            // SPI control byte for read
  RXBuffer[0] = SPI.transfer(0); // response code
  RXBuffer[1] = SPI.transfer(0); // length of data
  for (byte i = 0; i < RXBuffer[1]; i++)
    RXBuffer[i + 2] = SPI.transfer(0); // data
  if (RXBuffer[0] != 128 && readError != NULL)
    *readError = 1;
  digitalWrite(SS_PIN, HIGH);
  delay(1);
}

float Glucose_Reading(unsigned int val) { return ((val & 0x0FFF) / 8.5); }

/// Estabilishes communication with the module
/// Proceeds to next step: Module Configuration
void nfc_comm() {

  Serial.print("Checking NFC Module Communication...");

  byte code[3] = {0, 1, 0};
  sendCommand(code, 3);
  waitForResponse();
  spiRead(NULL);

  if ((RXBuffer[0] == 0) & (RXBuffer[1] == 15)) {
    Serial.println("OK");
    Serial.println("IDN COMMAND-"); //
    Serial.print("RESPONSE CODE: ");
    Serial.print(RXBuffer[0]);
    Serial.print(" LENGTH: ");
    Serial.println(RXBuffer[1]);
    Serial.print("DEVICE ID: ");
    byte i = 0;
    for (i = 2; i < (RXBuffer[1]); i++) {
      Serial.print(RXBuffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println(" ");
    Serial.print("ROM CRC: ");
    Serial.print(RXBuffer[RXBuffer[1]], HEX);
    Serial.print(RXBuffer[RXBuffer[1] + 1], HEX);
    Serial.println(" ");
    state = State::SetConfiguration;
  } else {
    Serial.println("ERROR");
    // lcd.clear();
    // lcd.print("NFC Module FAIL");
    state = State::StabilishComm;
  }
  delay(CMD_DELAY);
}

//############################################################################################################################

/// Sends command to config the module to communicate with Libre type tags
/// Proceeds to next step: Check for tags
void nfc_conf() {
  Serial.print("Configuring module ...");
  byte code[5] = {0x00, 0x02, 0x02, 0x01, 0x0D};
  sendCommand(code, 5);
  waitForResponse();

  { // Special read
    digitalWrite(SS_PIN, LOW);
    SPI.transfer(0x02);            // SPI control byte for read
    RXBuffer[0] = SPI.transfer(0); // response code
    RXBuffer[1] = SPI.transfer(0); // length of data
    digitalWrite(SS_PIN, HIGH);
  }

  if ((RXBuffer[0] == 0) & (RXBuffer[1] == 0)) // is response code good?
  {
    Serial.println("OK");
    state = State::Scan; // Go to next step: SCAN
  } else {
    Serial.println("ERROR");
    state = State::SetConfiguration; // Try again step: CONF
  }
  delay(CMD_DELAY);
}

//############################################################################################################################
/// Sends command to check for nearby tags
/// Proceeds to next step: extracting the value
void nfc_scan() {
  // step 1 send the command
  byte code[6] = {0x00, 0x04, 0x03, 0x26, 0x01, 0x00};
  sendCommand(code, 6);
  waitForResponse();
  spiRead(NULL);

  if (RXBuffer[0] == 128) // is response code good?
  {
    Serial.println("NFC Tag Found!");
    state = State::ScanFinished; // Read
  } else {
    Serial.println("Scanning....");
    nfc_sleep();
    state = State::Scan; // Scan Again
  }

  delay(CMD_DELAY);
}

//############################################################################################################################

/// This is where the magic happens
/// Glucose value is extracted
/// Better not touch for now ....
float nfc_get_glucose() {
  Serial.println("Extracting Glucose Value");
  byte oneBlock[8];
  String hexPointer = "";
  String trendValues = "";
  String hexMinutes = "";
  String elapsedMinutes = "";
  float trendOneGlucose;
  float trendTwoGlucose;
  float currentGlucose;
  float shownGlucose;
  float averageGlucose = 0;
  int glucosePointer;
  int validTrendCounter = 0;
  float validTrend[16];
  byte readError = 0;
  int readTry;
  int b;
  const char *hex = "0123456789ABCDEF";
  char str[24];

  int noDiffCount = 0;
  int sensorMinutesElapse;
  float lastGlucose;
  float trend[16];
  byte FirstRun = 1;

  for (b = 3; b < 16; b++) {
    readTry = 0;
    do {
      readError = 0;

      byte code[6] = {0x00, 0x04, 0x03, 0x02, 0x20, b};
      sendCommand(code, 6);
      waitForResponse();
      spiRead(&readError);

      //    Fills onBlock array with the current 'b' block data from the
      //    RXBuffer
      for (int i = 0; i < 8; i++)
        oneBlock[i] = RXBuffer[i + 3];

      byte *pin = oneBlock;

      char *pout = str;

      for (; pin < oneBlock + 8; pout += 2, pin++) {
        pout[0] = hex[(*pin >> 4) & 0xF];
        pout[1] = hex[*pin & 0xF];
      }
      pout[0] = 0;
      if (!readError) // is response code good?
      {
        Serial.println(str);
        trendValues += str;
      }
      readTry++;
    } while ((readError) && (readTry < MAX_NFC_READTRIES));
  }
  readTry = 0;
  do {
    readError = 0;
    //    Requests data from block 39
    byte code[6] = {0x00, 0x04, 0x03, 0x02, 0x20, 39};
    sendCommand(code, 6);
    waitForResponse();
    spiRead(&readError);

    for (int i = 0; i < 8; i++)
      oneBlock[i] = RXBuffer[i + 3];

    byte *pin = oneBlock;
    char *pout = str;
    for (; pin < oneBlock + 8; pout += 2, pin++) {
      pout[0] = hex[(*pin >> 4) & 0xF];
      pout[1] = hex[*pin & 0xF];
    }
    pout[0] = 0;
    if (!readError)
      elapsedMinutes += str;
    readTry++;
  } while ((readError) && (readTry < MAX_NFC_READTRIES));
  if (!readError) {
    hexMinutes =
        elapsedMinutes.substring(10, 12) + elapsedMinutes.substring(8, 10);
    hexPointer = trendValues.substring(4, 6);
    sensorMinutesElapse = strtoul(hexMinutes.c_str(), NULL, 16);
    glucosePointer = strtoul(hexPointer.c_str(), NULL, 16);
    String trendNow, trendTwo, trendOne;
    int ii = 0;
    for (int i = 8; i <= 200; i += 12) {
      if (glucosePointer == ii) {
        if (glucosePointer == 0) {
          trendNow =
              trendValues.substring(190, 192) + trendValues.substring(188, 190);
          trendOne =
              trendValues.substring(178, 180) + trendValues.substring(176, 178);
          trendTwo =
              trendValues.substring(166, 168) + trendValues.substring(164, 166);
          currentGlucose = Glucose_Reading(strtoul(trendNow.c_str(), NULL, 16));
          trendOneGlucose =
              Glucose_Reading(strtoul(trendOne.c_str(), NULL, 16));
          trendTwoGlucose =
              Glucose_Reading(strtoul(trendTwo.c_str(), NULL, 16));

          if (FirstRun == 1)
            lastGlucose = currentGlucose;

          if (((lastGlucose - currentGlucose) > 50) ||
              ((currentGlucose - lastGlucose) > 50)) {
            if (((lastGlucose - trendOneGlucose) > 50) ||
                ((trendOneGlucose - lastGlucose) > 50))
              currentGlucose = trendTwoGlucose;
            else
              currentGlucose = trendOneGlucose;
          }
        } else if (glucosePointer == 1) {
          trendNow = trendValues.substring(i - 10, i - 8) +
                     trendValues.substring(i - 12, i - 10);
          trendOne =
              trendValues.substring(190, 192) + trendValues.substring(188, 190);
          trendTwo =
              trendValues.substring(178, 180) + trendValues.substring(176, 178);
          currentGlucose = Glucose_Reading(strtoul(trendNow.c_str(), NULL, 16));
          trendOneGlucose =
              Glucose_Reading(strtoul(trendOne.c_str(), NULL, 16));
          trendTwoGlucose =
              Glucose_Reading(strtoul(trendTwo.c_str(), NULL, 16));

          if (FirstRun == 1)
            lastGlucose = currentGlucose;

          if (((lastGlucose - currentGlucose) > 50) ||
              ((currentGlucose - lastGlucose) > 50)) {
            if (((lastGlucose - trendOneGlucose) > 50) ||
                ((trendOneGlucose - lastGlucose) > 50))
              currentGlucose = trendTwoGlucose;
            else
              currentGlucose = trendOneGlucose;
          }
        } else {
          trendNow = trendValues.substring(i - 10, i - 8) +
                     trendValues.substring(i - 12, i - 10);
          trendOne = trendValues.substring(i - 22, i - 20) +
                     trendValues.substring(i - 24, i - 22);
          trendTwo = trendValues.substring(i - 34, i - 32) +
                     trendValues.substring(i - 36, i - 34);
          currentGlucose = Glucose_Reading(strtoul(trendNow.c_str(), NULL, 16));
          trendOneGlucose =
              Glucose_Reading(strtoul(trendOne.c_str(), NULL, 16));
          trendTwoGlucose =
              Glucose_Reading(strtoul(trendTwo.c_str(), NULL, 16));

          if (FirstRun == 1)
            lastGlucose = currentGlucose;

          if (((lastGlucose - currentGlucose) > 50) ||
              ((currentGlucose - lastGlucose) > 50)) {
            if (((lastGlucose - trendOneGlucose) > 50) ||
                ((trendOneGlucose - lastGlucose) > 50))
              currentGlucose = trendTwoGlucose;
            else
              currentGlucose = trendOneGlucose;
          }
        }
      }

      ii++;
    }

    for (int i = 8, j = 0; i < 200; i += 12, j++) {
      String t =
          trendValues.substring(i + 2, i + 4) + trendValues.substring(i, i + 2);
      trend[j] = Glucose_Reading(strtoul(t.c_str(), NULL, 16));
    }

    for (int i = 0; i < 16; i++) {
      if (((lastGlucose - trend[i]) > 50) ||
          ((trend[i] - lastGlucose) > 50)) // invalid trend check
        continue;
      else {
        validTrend[validTrendCounter] = trend[i];
        validTrendCounter++;
      }
    }

    if (validTrendCounter > 0) {
      for (int i = 0; i < validTrendCounter; i++)
        averageGlucose += validTrend[i];

      averageGlucose = averageGlucose / validTrendCounter;

      if (((lastGlucose - currentGlucose) > 50) ||
          ((currentGlucose - lastGlucose) > 50))
        shownGlucose = averageGlucose; // If currentGlucose is still invalid
                                       // take the average value
      else
        shownGlucose =
            currentGlucose; // All went well. Take and show the current value
    } else
      shownGlucose = currentGlucose; // If all is going wrong, nevertheless take
                                     // and show a value

    if ((lastGlucose == currentGlucose) &&
        (sensorMinutesElapse > 21000)) // Expired sensor check
      noDiffCount++;

    if (lastGlucose != currentGlucose) // Reset the counter
      noDiffCount = 0;

    if (currentGlucose != 0)
      lastGlucose = currentGlucose;

    state = State::Scan;
    FirstRun = 0;

    if (noDiffCount > 5)
      return 0;
    else {
      Serial.print("Trend 1: ");
      Serial.println(trendOneGlucose);
      Serial.print("Trend 2: ");
      Serial.println(trendTwoGlucose);

      Serial.print("Current: ");
      Serial.println(currentGlucose);
      return shownGlucose;
    }
  } else {
    Serial.print("ERROR in Reading NFC Tag");
    state = State::Scan; // try again step: SCAN
    readError = 0;
  }
  return 0;
}
//############################################################################################################################

void nfc_startup() {

  // Send module Wake-up pulse
  Serial.println("Waking up the Module");
  pinMode(IRQ_PIN, OUTPUT);
  digitalWrite(IRQ_PIN, HIGH);
  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);

  // SPI Init and Configuration
  Serial.println("SPI Init and Config");
  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV32);
}
#line 1 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\nvs.ino"
#include "nvs.h"
#include "nvs_flash.h"

void startNVS() {
  esp_err_t err;
  err = nvs_flash_init();
  if (err == ESP_OK) {
    nvs_handle nvshandle;
    err = nvs_open("data", NVS_READWRITE, &nvshandle);
    if (err == ESP_OK) {
      int32_t nvs_index = 0;
      err = nvs_get_i32(nvshandle, "index", &nvs_index);
      if (err == ESP_ERR_NVS_NOT_FOUND) {
        Serial.println("Not found");
      } else if (err == ESP_OK) {
        Serial.println("Found");
        Serial.print("index: "); Serial.println(nvs_index);
        nvs_index++;
       }
        err = nvs_set_i32(nvshandle, "index", nvs_index);
        if (err == ESP_OK) {
          Serial.println("Written into handle");
          err = nvs_commit(nvshandle);
          if (err == ESP_OK) Serial.println("Written to Flash");
        }

      
    }

  }
}


#line 1 "c:\\Users\\Leonardo\\Documents\\Arduino\\ES32-BM019-BLE-DEEPSLEEP-TABS-TTGO-NVS\\pwr.ino"
void nfc_sleep() {
  delay(TIME_TO_SLEEP-1 * 1000);
}

