//
// Created by Kurosu Chan on 2022/8/5.
//
#include "StripCommon.h"

void ColorCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 3) {
    // BGR
    auto color = Adafruit_NeoPixel::Color(data[0], data[1], data[2]);
    strip.color = color;
    [[maybe_unused]]
    auto size = strip.pref.putUInt("color", strip.color);
//    auto c = strip.pref.getUInt("color", Adafruit_NeoPixel::Color(255, 255, 255));
//    printf("[ColorCharCallback] color stored in pref: %x, size: %d\n", c, size);
  } else {
    ESP_LOGE("ColorCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.color);
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
    strip.setMaxLeds(max_leds);
    strip.pref.putInt("max_leds", max_leds);
  } else {
    ESP_LOGE("LengthCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.max_leds);
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

HaltDelayCharCallback::HaltDelayCharCallback(Strip &strip): strip(strip) {}
