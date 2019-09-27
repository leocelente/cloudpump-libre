#include "nvs.h"
#include "nvs_flash.h"
#define INDEX_KEY "index"
#define DATA_SIZE 14 // = 10 (timestamp) + 3 (glucose) + 1 (\0)

nvs_handle data_handle;
esp_err_t e;

bool nvs_startup() {
  e = nvs_flash_init();
  if (e == ESP_OK) {
    e = nvs_open("data", NVS_READWRITE, &data_handle);
    return (e == ESP_OK);
  } else
    return e;
}

bool set_index(int index_) {
  e = nvs_set_i32(data_handle, INDEX_KEY, index_);
  if (e == ESP_OK) {
    e = nvs_commit(data_handle);
    return (e == ESP_OK)
  } else
    return false;
}

bool index_cached = false;
int cached = 0;

int get_index() {
  if(index_cached) return cached;
  int32_t i = 0;
  e = nvs_get_i32(data_handle, INDEX_KEY, &i);
  if (e == ESP_ERR_NVS_NOT_FOUND) { // Handle First start (empty data)
    set_index(0);
  } else if (e == ESP_OK) {
    cached = i;
    index_cached = true;
    return i;
  } else
    return i;
}
struct Data {
  int time;
  int value;
  Data(int time_, int value_):time(time_),value(value_);
  Data(char* data){
    s_data= String(data);
    time = s_data.substring(0,9).toInt();
    value = s_data.substring(10).toInt();
  }
  char* as_char(){
    return String(String(time) +String(value)).c_str();
  }
}
Data get_value(int index_){
  char* value = malloc(DATA_SIZE);
  size_t size = DATA_SIZE;
  char* index = String(index_).c_str();
  e = nvs_get_str(data_handle, index, &value, &size);
  if(e == ESP_OK){
    return Data(value);
  }else return Data(0,0);

}

Data* get_values() {
  int index = get_index();
  Data* values = new Data[index + 1] [DATA_SIZE]();
  for (int i = 0; i < index; i++){
    values[i] = get_value(i);
  }
  return values;
}

bool set_value(int index_, int value) {
  char *key = String(index_).c_str();

  int timestamp = rtc_get_time();

  String s_data = String(timestamp) + String(value);

  char *data = s_data.c_str(); // 10 timestamp + 3 glucose + \0

  e = nvs_set_str(data_handle, key, data);

  if (e == ESP_OK) {
    e = nvs_commit(data_handle);
    return (e == ESP_OK);
  } else
    return false;
}

bool nvs_save(int glucose) {
  int index = get_index();
  return set_value(index, glucose)
}

void nvs_shutdown(){
  nvs_close(data_handle);
}