//
// Created by Kurosu Chan on 2022/8/5.
//
#include "StripCommon.h"
#include "StripCallbacks.h"


void BrightnessCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 1) {
    uint8_t brightness = data[0];
    strip.setBrightness(brightness);
    [[maybe_unused]]
    auto size = strip.pref.putUChar("brightness", strip.brightness);
    ESP_LOGI("BrightnessCharCallback", "Brightness changed to %d", brightness);
  } else {
    ESP_LOGE("BrightnessCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.pixels->getBrightness());
  }
}

void StatusCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  uint8_t status = data[0];
  if (status < StripStatus_LENGTH) {
    auto s = StripStatus(status);
    strip.status = s;
    ESP_LOGI("StatusCharCallback", "status changed to %d", status);
  } else {
    ESP_LOGE("StatusCharCallback", "Invalid status: %d", status);
    characteristic->setValue(strip.status);
  }
}

void ConfigCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (strip.status != StripStatus::STOP){
    ESP_LOGE("ConfigCharCallback", "Strip is not stopped, cannot change config");
    characteristic->setValue(1);
    return;
  }
  auto decode_tuple_list = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
    auto &tuple_list = *(reinterpret_cast<std::vector<TupleIntFloat> *>(*arg)); // ok as reference
    TupleIntFloat t = TupleIntFloat_init_zero;
    bool status = pb_decode(stream, TupleIntFloat_fields, &t);
    if (status) {
      tuple_list.emplace_back(t);
      return true;
    } else {
      return false;
    }
  };
  TrackConfig config = TrackConfig_init_zero;
  pb_istream_t istream = pb_istream_from_buffer(data, data.length());
  // https://stackoverflow.com/questions/7774938/in-c-will-the-vector-function-push-back-increase-the-size-of-an-empty-array
  auto received_tuple = std::vector<TupleIntFloat>{};
  received_tuple.reserve(15);
  config.lst.arg = reinterpret_cast<void *>(&received_tuple);
  config.lst.funcs.decode = decode_tuple_list;
  bool success = pb_decode(&istream, TrackConfig_fields, &config);
  if (!success) {
    ESP_LOGE("Decode Config", "Error: Something goes wrong when decoding");
    return;
  }
  auto m = std::map<int, float>{};
  for (auto &t: received_tuple) {
    m.insert_or_assign(t.dist, t.speed);
  }
  // prevent additional copy
  auto retriever = ValueRetriever<float>(std::move(m));
  auto track = Track{
      .id = config.id,
      .state = RunState{0, 0, 0, false},
      .retriever = std::move(retriever),
      .color = Adafruit_NeoPixel::Color(config.color.red, config.color.green, config.color.blue),
  };
  if (config.command == Command_ADD) {
    ESP_LOGI("ConfigChar", "Add track id %d", track.id);
    bool isDuplicated = std::find_if(strip.tracks.begin(), strip.tracks.end(), [&track](const Track &t) {
      return t.id == track.id;
    }) != strip.tracks.end();
    // If not present, it returns an iterator to one-past-the-end.
    if (isDuplicated) {
      ESP_LOGE("ConfigChar", "Duplicated track id %d", track.id);
      return;
    }
    strip.tracks.emplace_back(std::move(track));
    // sort the tracks by id from small to large.
    std::sort(strip.tracks.begin(), strip.tracks.end(), [](const Track &a, const Track &b) {
      return a.id < b.id;
    });
    // set the value of the characteristic to zero after finishing the operation.
    strip.config_char->setValue(0);
  } else if (config.command == Command_RESET) {
    ESP_LOGI("ConfigChar", "Reset and add track id %d", track.id);
    strip.tracks.clear();
    strip.tracks.emplace_back(std::move(track));
    strip.config_char->setValue(0);
  } else {
    ESP_LOGE("ConfigCharCallback", "Invalid command: %d", config.command);
  }
}

void MaxLEDsCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 2) {
    /** TODO: make a function to convert uint8 array to uint16_t
     *   both big endian and little endian.
    **/
    // ESP32 is little endian.
    // NOTE: the sender should send the data in little endian.
    // i.e. the first byte is the least significant byte.
    constexpr uint16_t MAX_MAX_LEDs = 5000;
    uint16_t max_LEDs = data[0] | data[1] << 8;
    if (max_LEDs < MAX_MAX_LEDs) {
      if (strip.status != StripStatus::STOP){
        ESP_LOGE("MaxLEDsCharCallback", "Strip is running. You SHOULD NOT change the max LEDs.");
        return;
      }
      ESP_LOGI("MaxLEDsCharCallback", "Max LEDs changed to %d", max_LEDs);
      strip.setMaxLEDs(max_LEDs);
    } else {
      ESP_LOGE("MaxLEDsCharCallback", "Invalid max LEDs: %d. Should less than %d", max_LEDs, MAX_MAX_LEDs);
    }
  } else {
    ESP_LOGE("MaxLEDsCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.max_LEDs);
  }
}
