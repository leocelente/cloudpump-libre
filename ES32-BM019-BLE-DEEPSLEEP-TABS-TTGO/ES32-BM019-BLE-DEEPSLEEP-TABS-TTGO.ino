#include <time.h>

/* Time ESP32 will go to sleep (in seconds) */

enum class Mode {
  Normal, Advertise
};

void why_reset() {
  int reset_reason = rtc_get_reset_reason(0);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting....");
  // Serial.print("Size Of Char: ");
  // Serial.println(sizeof(char));

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Mode mode{Mode::Normal};
  mode = selectMode(cause);
  nvs_startup();
  if(mode == Mode::Normal){
    int glucose = extract_glucose();
    nvs_save(glucose);
    pwr_sleep();
  } else if(mode == Mode::Advertise){
    ble_startup();
  }


  //  routine();
  //  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //  Serial.println("Starting deep sleep...");

  //  esp_deep_sleep_start();
}
void loop(){}
void loop_() {
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
  } else
    state = State::StabilishComm;
  //  else if (NFCReady == BLE_SYNC) ble_sync();
  delay(1000);
}
