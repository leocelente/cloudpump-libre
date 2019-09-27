#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
// #include <string>

#define BLE_NAME "CGM32"

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

#define CMD_ADVERTISE 'a'
#define CMD_STOP_ADV 'c'
#define CMD_PRINT 'p'
#define CMD_WRITE 'w'

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

bool advertise = false;
bool sync = false;

enum class Command {
  Sync,
  Advertise,
  StopAd,
  Print,
  Write,
  Invalid
};

Command parseCmd(String cmd ){
  if(cmd.indexOf('s') != -1) return Command::Sync;
  else if(cmd.indexOf(CMD_ADVERTISE) != -1) return Command::Advertise;
  else if(cmd.indexOf(CMD_STOP) != -1) return Command::StopAd;
  else if(cmd.indexOf(CMD_PRINT) != -1) return Command::Print;
  else if(cmd.indexOf(CMD_WRITE) != -1) return Command::Write;
  else return Command::Invalid;
}

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
  void onWrite(BLECharacteristic *ad_){}
  void onRead(BLECharacteristic *ad_){}
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic_) {
    Serial.println("RX being written to");
    std::string rxValue = characteristic_->getValue();
    Command cmd = parseCmd(String(characteristic_->getValue()));
    switch (cmd)
    {
      case Command::Sync:
        /* code */
        sync = true;
        tx->setValue();
        break;

      case Command::Advertise:
          advertise = true;
        break;

      case Command::StopAd:
          advertise = false;
        break;

      case Command::Print:
          // NVS GET ALL DATA
         Data* datas =  nvs_get_values();
         int size = get_index() + 1;
         for(int i = 0; i < size; i++)
            Serial.println(datas[i].as_char());
        break;

      case Command::Write:
          int index = get_index();
          //String(rxValue).split(' ')[1]; // GET VALUE FROM CMD
          set_value(index, 123);
        break;

      case Command::Invalid:
      default:
        /* code */
        tx->setValue("Invalid");
        break;
    }
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
  void onRead(BLECharacteristic *tx_) {
    Serial.print("Reading number: ");
    Serial.println(c);
    String t = String(getTime());
    String value = t + String(readings[c]);
    tx_->setValue(value.c_str());
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

void ble_startup() {
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
  return 1540508787;
}
int rtc_get_time() {
  return 1540508787;
}

uint8_t ble_convert(float gluc) {
  int iValue = static_cast<int>(gluc);
  return (uint8_t)iValue;
}
