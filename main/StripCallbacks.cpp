//
// Created by Kurosu Chan on 2022/8/5.
//
#include "StripCommon.h"
#include "StripCallbacks.h"

bool decode_map(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    auto m = (reinterpret_cast<std::map<int, float> *>(*arg));
    TrackData_PaceEntry t = TrackData_PaceEntry_init_zero;
    bool status = pb_decode(stream, TrackData_PaceEntry_fields, &t);
    if (status) {
        m->insert_or_assign(t.key, t.value);
        return true;
    } else {
        return false;
    }
}

void BrightnessCharCallback::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) {
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

void StatusCharCallback::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) {
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

void ConfigCharCallback::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) {
    auto data = characteristic->getValue();
    ESP_LOGI("ConfigCharCallback", "Received %d bytes", data.length());
    if (strip.status != TrackStatus_STOP) {
        ESP_LOGE("ConfigCharCallback", "Strip is not stopped, cannot change config");
        characteristic->setValue(1);
        return;
    }
    // big endian
    uint16_t total = data[0] << 8u | data[1];
    uint8_t count = data[2];
    uint8_t current_len = data[3];
    ESP_LOGI("ConfigCharCallback", "total: %d, count: %d, current_len: %d", total, count, current_len);

    if (total > STRIP_DECODE_BUFFER_SIZE) {
        ESP_LOGE("ConfigCharCallback", "Invalid total: %d", total);
        characteristic->setValue(1);
        reset();
        return;
    }

    if (last_count + 1 != count) {
        ESP_LOGE("ConfigCharCallback", "Invalid count: %d, last_count: %d", count, last_count);
        characteristic->setValue(1);
        reset();
        return;
    }

    if (count != 0 && last_total != total) {
        ESP_LOGE("ConfigCharCallback", "Invalid total: %d, last_total: %d", total, last_total);
        characteristic->setValue(1);
        reset();
        return;
    }

    auto start = data.begin() + 4;
    auto end = start + current_len;
    std::copy(start, end, strip.decode_buffer.begin() + last_offset);
    last_total = total;
    last_count = count;
    last_offset += current_len;
    ESP_LOGI("ConfigCharCallback", "Received %d/%d", last_offset, last_total);
    if (last_offset == last_total) {
        strip.tracks.clear();
        auto decode_tracks = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
            auto &tracks_list = *(reinterpret_cast<std::vector<Track> *>(*arg));
            auto m = std::map<int, float>{};
            TrackData t = TrackData_init_zero;
            t.pace.arg = reinterpret_cast<void *>(&m);
            t.pace.funcs.decode = decode_map;
            bool status = pb_decode(stream, TrackData_fields, &t);
            if (status) {
                if (m.empty()){
                    ESP_LOGE("ConfigCharCallback", "empty pace");
                } else {
                    for (auto &p: m) {
                        ESP_LOGD("ConfigCharCallback", "id:%ld, key: %d, value: %f", t.id, p.first, p.second);
                    }
                }
                auto retriever = ValueRetriever<float>(std::move(m));
                tracks_list.emplace_back(Track{
                        .id = t.id,
                        .state = RunState{0, 0, 0, false},
                        .retriever = std::move(retriever),
                        .color = Adafruit_NeoPixel::Color(t.color.red, t.color.green, t.color.blue),
                });
                return true;
            } else {
                return false;
            }
        };
        TrackConfig config = TrackConfig_init_zero;
        pb_istream_t istream = pb_istream_from_buffer(strip.decode_buffer.begin(), total);
        config.tracks.arg = reinterpret_cast<void *>(&strip.tracks);
        config.tracks.funcs.decode = decode_tracks;
        bool success = pb_decode(&istream, TrackConfig_fields, &config);
        if (!success) {
            characteristic->setValue(1);
            ESP_LOGE("ConfigCharCallback", "decode nanopb");
        } else {
            characteristic->setValue(0);
            ESP_LOGI("ConfigCharCallback", "tracks size: %d", strip.tracks.size());
        }
        reset();
    }
}

void ConfigCharCallback::reset() {
    last_count = -1;
    last_total = 0;
    last_offset = 0;
    this->strip.resetDecodeBuffer();
}

void OptionsCharCallback::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) {
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
      ESP_LOGI("OptionsCharCallback", "max_led_length changed to %lu", strip.max_LEDs);
      break;
    }
    case TrackOptions_count_led_tag: {
      auto c = options.options.count_led.num;
      strip.setCountLEDs(c);
      strip.pref.putUInt(STRIP_TRACK_LEDs_NUM_KEY, c);
      ESP_LOGI("OptionsCharCallback", "count_led changed to %lu", strip.count_LEDs);
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
