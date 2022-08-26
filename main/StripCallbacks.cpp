//
// Created by Kurosu Chan on 2022/8/5.
//
#include "StripCommon.h"


void ColorCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (data.length() >= 3) {
    // BRG
    for(uint8_t i=0;i<3;i++){
      auto color = Adafruit_NeoPixel::Color(data[i], data[i+1], data[i+2]);
      strip.color[i] = color;
    }
    
    [[maybe_unused]]
    auto size = strip.pref.putUInt("color0", strip.color[0]);
    auto size = strip.pref.putUInt("color1", strip.color[1]);
    auto size = strip.pref.putUInt("color2", strip.color[2]);
    
//    auto c = strip.pref.getUInt("color", Adafruit_NeoPixel::Color(255, 255, 255));
//    printf("[ColorCharCallback] color stored in pref: %x, size: %d\n", c, size);
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

HaltDelayCharCallback::HaltDelayCharCallback(Strip &strip): strip(strip) {}

void speedCharCallback::onWrite(NimBLECharacteristic *characteristic) {
  auto data = characteristic->getValue();
  if (50 > data.length() >= 10 ) {
    for(uint8_t i=0;i<8;i++){
      speed_female[i*100] = data[2*i] | data[2*i+1] <<8;
      speed_female[i*100+1] = data[2*i+16] | data[2*i+17] <<8;
      speed_female[i*100+2] = data[2*i+32] | data[2*i+33] <<8;
      auto size = strip.pref.putUInt("speed_female["+i*100, speed_female[i*100]);
      auto size = strip.pref.putUInt("speed_female["+(i*100+1), speed_female[i*100+1]);
      auto size = strip.pref.putUInt("speed_female["+(i*100+2), speed_female[i*100+2]);
    }
    characteristic->setValue(speed_female);
    characteristic->notify();
  } 
  else if(data.length() < 70){
    for(uint8_t i=0;i<10;i++){
      speed_male[i*100] = data[2*i] | data[2*i+1] <<8;
      speed_male[i*100+1] = data[2*i+20] | data[2*i+17] <<8;
      speed_male[i*100+2] = data[2*i+40] | data[2*i+33] <<8;
      auto size = strip.pref.putUInt("speed_male["+i*100, speed_male[i*100]);
      auto size = strip.pref.putUInt("speed_male["+(i*100+1), speed_male[i*100+1]);
      auto size = strip.pref.putUInt("speed_male["+(i*100+2), speed_male[i*100+2]);
    }
    characteristic->setValue(speed_female);
    characteristic->notify();
  }
  else {
    ESP_LOGE("speedCharCallback", "Invalid data length: %d", data.length());
    // characteristic->setValue(strip.halt_delay);
  }
}

speedCharCallback::speedCharCallback(Strip &strip): strip(strip) {}