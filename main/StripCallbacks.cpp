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
    auto size = strip.pref.putUChar(STRIP_BRIGHTNESS_KEY, strip.brightness);
    ESP_LOGI("BrightnessCharCallback", "Brightness changed to %d", brightness);
  } else {
    ESP_LOGE("BrightnessCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.pixels->getBrightness());
  }
}

void StatusCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  TrackStatusMsg msg = TrackStatusMsg_init_zero;
  pb_istream_t istream = pb_istream_from_buffer(data, data.length());
  auto res = pb_decode(&istream, TrackStatusMsg_fields, &msg);
  if (!res) {
    ESP_LOGE("Decode Options", "Error: Something goes wrong when decoding");
    return;
  }
  auto status = msg.status;
  strip.status = status;
}

void ConfigCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (strip.status != TrackStatus_STOP) {
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

void OptionsCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (strip.status != TrackStatus_STOP) {
    ESP_LOGE("OptionsCharCallback", "Strip is not stopped, cannot change options");
    characteristic->setValue(0);
    return;
  }
  TrackOptions options = TrackOptions_init_zero;
  pb_istream_t istream = pb_istream_from_buffer(data, data.length());
  auto res = pb_decode(&istream, TrackOptions_fields, &options);
  if (!res) {
    ESP_LOGE("Decode Options", "Error: Something goes wrong when decoding");
    return;
  }
  switch (options.which_options) {
    case TrackOptions_max_led_tag: {
      auto l = options.options.max_led.num;
      strip.setMaxLEDs(l);
      strip.pref.putUInt(STRIP_CIRCLE_LEDs_NUM_KEY, l);
      ESP_LOGI("OptionsCharCallback", "max_led_length changed to %d", strip.max_LEDs);
      break;
    }
    case TrackOptions_count_led_tag: {
      auto c = options.options.count_led.num;
      strip.setCountLEDs(c);
      strip.pref.putUInt(STRIP_TRACK_LEDs_NUM_KEY, c);
      ESP_LOGI("OptionsCharCallback", "count_led changed to %d", strip.count_LEDs);
      break;
    }
    case TrackOptions_circle_tag: {
      auto c = options.options.circle.meter;
      strip.setCircleLength(c);
      ESP_LOGI("OptionsCharCallback", "circle_length changed to %f", c);
      break;
    }
  }
}
