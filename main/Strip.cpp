//
// Created by Kurosu Chan on 2022/7/13.
//

#include "Strip.h"


void Strip::stripTask() const {
  pixels->clear();
  pixels->show();
  for(;;){
    for (int i = 0; i < max_leds; i++) {
      // Stupid OEM
      // RGB -> BRG
      pixels->clear();
      pixels->fill(color, i, 10);
      pixels->show();
      vTaskDelay(delay_ms / portTICK_PERIOD_MS);
    }
  }
}

Strip::Strip(int max_leds, int pin){
  this->max_leds = max_leds;
  this->pin = pin;
  pixels = new Adafruit_NeoPixel(max_leds, pin, NEO_GRB + NEO_KHZ800);
  pixels->begin();
  pixels->setBrightness(32);
}

/// update max length of strip.
void Strip::updateMaxLength(int new_max_leds) {
  max_leds = new_max_leds;
  delete pixels;
  pixels = new Adafruit_NeoPixel(max_leds, pin, NEO_GRB + NEO_KHZ800);
  pixels->begin();
}

void Strip::registerBLE(NimBLEServer *server) {
  auto pService = server->createService(LIGHT_SERVICE_UUID);
  auto color_char = pService->createCharacteristic(LIGHT_CHAR_COLOR_UUID,
                                                                   NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  auto colorCharCallback = new ColorCharCallback(*this);
  color_char->setValue(color);
  color_char->setCallbacks(colorCharCallback);

  auto brightness_char = pService->createCharacteristic(LIGHT_CHAR_BRIGHTNESS_UUID,
                                                                   NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  auto brightnessCharCallback = new BrightnessCharCallback(*this);
  brightness_char->setValue(32);
  brightness_char->setCallbacks(brightnessCharCallback);

  auto delay_char = pService->createCharacteristic(LIGHT_CHAR_DELAY_UUID,
                                                                   NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  auto delayCharCallback = new DelayCharCallback(*this);
  delay_char->setValue(delay_ms);
  delay_char->setCallbacks(delayCharCallback);
  pService->start();
}

void ColorCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 3) {
//    uint32_t color = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    auto color = Adafruit_NeoPixel::Color(data[0], data[1], data[2]);
    strip.color = color;
  } else {
    ESP_LOGE("ColorCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.color);
  }
}
ColorCharCallback::ColorCharCallback(Strip &strip):strip(strip) {}

void BrightnessCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 1) {
    uint8_t brightness = data[0];
    strip.pixels->setBrightness(brightness);
  } else {
    ESP_LOGE("BrightnessCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.pixels->getBrightness());
  }
}
BrightnessCharCallback::BrightnessCharCallback(Strip &strip):strip(strip) {}

void DelayCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 2) {
    uint16_t delay_ms = data[0] << 8 | data[1];
    strip.delay_ms = delay_ms;
  } else {
    ESP_LOGE("DelayCharCallback", "Invalid data length: %d", data.length());
    characteristic->setValue(strip.delay_ms);
  }
}
DelayCharCallback::DelayCharCallback(Strip &strip):strip(strip) {}
