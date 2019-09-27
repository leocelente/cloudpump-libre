#define uS_TO_S_FACTOR                                                         \
  1000000               /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 5 

void nfc_sleep() {
  // delay(TIME_TO_SLEEP-1 * 1000);
  // SEND CMD TO PUT MODULE IN HIBERNATION
}

Mode selectMode(esp_wakeup_cause_t wakeup_cause){
  
  if(wakeup_cause == ESP_SLEEP_WAKEUP_TIMER)
  return Mode::Normal;
  else {
    int reset_reason = rtc_get_reset_reason(0);
    // return Mode::Advertise;
    return Mode::Normal;
  }
}

void ble_shutdown(){}

void pwr_sleep(){
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  nfc_sleep();
  ble_shutdown();
  esp_deep_sleep_start();
}
