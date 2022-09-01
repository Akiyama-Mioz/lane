//
// Created by Kurosu Chan on 2022/8/5.
//
#include "StripCommon.h"


void ColorCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 3) {
    // BRG
    for (uint8_t i = 0; i < 3; i++) {
      auto color = Adafruit_NeoPixel::Color(data[i], data[i + 1], data[i + 2]);
      strip.color[i] = color;
    }
    strip.pref.putUInt("color0", strip.color[0]);
    strip.pref.putUInt("color1", strip.color[1]);
    strip.pref.putUInt("color2", strip.color[2]);
  } else {
    ESP_LOGE("ColorCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.color[0]);
  }
}

ColorCharCallback::ColorCharCallback(Strip &strip) : strip(strip) {}

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
//    printf("[BrightnessCharCallback] brightness stored in pref: %d, size: %d\n", brightness, size);
  } else {
    ESP_LOGE("BrightnessCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.pixels->getBrightness());
  }
}

BrightnessCharCallback::BrightnessCharCallback(Strip &strip) : strip(strip) {}

void DelayCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 2) {
    uint16_t delay_ms = data[0] | data[1] << 8;
    strip.delay_ms = delay_ms;
  } else {
    ESP_LOGE("DelayCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.delay_ms);
  }
}

DelayCharCallback::DelayCharCallback(Strip &strip) : strip(strip) {}

void MaxLEDsCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 2) {
    /** TODO: make a function to convert uint8 array to uint16_t
     *   both big endian and little endian.
    **/
    uint16_t max_leds = data[0] | data[1] << 8;
    strip.setMaxLEDs(max_leds);
    strip.pref.putInt("max_leds", max_leds);
  } else {
    ESP_LOGE("LengthCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.max_LEDs);
  }
}

MaxLEDsCharCallback::MaxLEDsCharCallback(Strip &strip) : strip(strip) {}


void StatusCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  uint8_t status = data[0];
  if (status < StripStatus_LENGTH) {
    auto s = StripStatus(status);
    strip.status = s;
  } else {
    ESP_LOGE("StatusCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.status);
  }
}

StatusCharCallback::StatusCharCallback(Strip &strip) : strip(strip) {}

void HaltDelayCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 2) {
    uint16_t delay_ms = data[0] | data[1] << 8;
    strip.halt_delay = delay_ms;
  } else {
    ESP_LOGE("HaltDelayCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.halt_delay);
  }
}

HaltDelayCharCallback::HaltDelayCharCallback(Strip &strip) : strip(strip) {}

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

ValueRetriever<float> getValueRetriever(NimBLEAttValue &data) {
  TrackConfig config_decoded = TrackConfig_init_zero;
  pb_istream_t istream = pb_istream_from_buffer(data, data.length());   ///encode -> imsg
  auto received_tuple = std::vector<TupleIntFloat>{};
  config_decoded.lst.arg = reinterpret_cast<void *>(&received_tuple);
  config_decoded.lst.funcs.decode = decode_tuple_list;
  bool status_decode = pb_decode(&istream, TrackConfig_fields, &config_decoded);
  if (!status_decode) {
    ESP_LOGE("Decode speed0", "Error: Something goes wrong when decoding");
  }
  auto m = std::map<int, float>{};
  for (auto &t: received_tuple) {
    m.insert_or_assign(t.dist, t.speed);
  }
  return ValueRetriever<float>(m);
}

void SpeedCustomCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  strip.speedCustom = getValueRetriever(data);
}

void SpeedCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  strip.speedVals[index] = getValueRetriever(data);
}
