//
// Created by Kurosu Chan on 2022/8/5.
//
#include "StripCommon.h"

void BrightnessCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 1) {
    uint8_t brightness = data[0];
    strip.brightness = brightness;
    // update the brightness of the strip.
    // it may be nullptr during update max length.
    if (strip.pixels != nullptr) {
      strip.pixels->setBrightness(brightness);
    }
    [[maybe_unused]]
    auto size = strip.pref.putUChar("brightness", strip.brightness);
    ESP_LOGI("BrightnessCharCallback", "Brightness changed to %d", brightness);
  } else {
    ESP_LOGE("BrightnessCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.pixels->getBrightness());
  }
}

BrightnessCharCallback::BrightnessCharCallback(Strip &strip) : strip(strip) {}

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

StatusCharCallback::StatusCharCallback(Strip &strip) : strip(strip) {}

void ConfigCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
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
  // 20 should be more than enough.
  // https://stackoverflow.com/questions/7774938/in-c-will-the-vector-function-push-back-increase-the-size-of-an-empty-array
  auto received_tuple = std::vector<TupleIntFloat>{};
  received_tuple.reserve(11);
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
    auto isDuplicated = std::find_if(strip.tracks.begin(), strip.tracks.end(), [&track](const auto &t) {
      return t.id == track.id;
    });
    // If not present, it returns an iterator to one-past-the-end.
    if (isDuplicated != strip.tracks.end()) {
      ESP_LOGE("ConfigChar", "Duplicated track id %d", track.id);
      return;
    }
    strip.tracks.emplace_back(track);
    // sort the tracks by id from small to large.
    std::sort(strip.tracks.begin(), strip.tracks.end(), [](const Track &a, const Track &b) {
      return a.id < b.id;
    });
    // set the value of the characteristic to zero after finishing the operation.
    strip.config_char->setValue(0);
  } else if (config.command == Command_RESET) {
    ESP_LOGI("ConfigChar", "Reset and add track id %d", track.id);
    strip.tracks.clear();
    strip.tracks.emplace_back(track);
    strip.config_char->setValue(0);
  } else {
    ESP_LOGE("ConfigCharCallback", "Invalid command: %d", config.command);
  }
}
