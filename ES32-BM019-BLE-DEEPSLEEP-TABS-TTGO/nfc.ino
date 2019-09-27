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

// bitwise  & is a simple mask (& F0F deletes the middle digit)
float Glucose_Reading(unsigned int val) { return ((val & 0x0FFF) / 8.5); }

/// Estabilishes communication with the module
/// Proceeds to next step: Module Configuration
bool nfc_comm() {

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
    // state = State::SetConfiguration;
    return true;
  } else {
    Serial.println("ERROR");
    // lcd.clear();
    // lcd.print("NFC Module FAIL");
    // state = State::StabilishComm;
    return false;
  }
  delay(CMD_DELAY);
}

//############################################################################################################################

/// Sends command to config the module to communicate with Libre type tags
/// Proceeds to next step: Check for tags
bool nfc_conf() {
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
    // state = State::Scan; // Go to next step: SCAN
    return true;
  } else {
    Serial.println("ERROR");
    // state = State::SetConfiguration; // Try again step: CONF
    return false;
  }
  delay(CMD_DELAY);
}

//############################################################################################################################
/// Sends command to check for nearby tags
/// Proceeds to next step: extracting the value
bool nfc_scan() {
  // step 1 send the command
  byte code[6] = {0x00, 0x04, 0x03, 0x26, 0x01, 0x00};
  sendCommand(code, 6);
  waitForResponse();
  spiRead(NULL);

  if (RXBuffer[0] == 128) // is response code good?
  {
    Serial.println("NFC Tag Found!");
    // state = State::ScanFinished; // Read
    return true;
  } else {
    Serial.println("Scanning....");
    nfc_sleep();
    // state = State::Scan; // Scan Again
    return false;
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

      // Builds str with oneBlock
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
    if (!readError) {
      elapsedMinutes += str;
    }
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

          if (((lastGlucose - currentGlucose) > 50)     || ((currentGlucose - lastGlucose) > 50)) {
            if (((lastGlucose - trendOneGlucose) > 50)  || ((trendOneGlucose - lastGlucose) > 50))
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

int extract_glucose() {
  bool ok = false;
  nfc_startup();
  ok = nfc_comm();
  if (ok)
    ok = nfc_conf();
  if (ok)
    ok = nfc_scan();
  if (ok) {
    int nGlucose = static_cast<int>(nfc_get_glucose());
    return nGlucose;
  } else
    return false;
}

// void nfc_sleep(){}
