//
// Created by Kurosu Chan on 2022/7/13.
//

#include "Strip.h"

void Strip::fillForward(int fill_count = 10) const {
  for (int i = 0; i < max_leds; i++) {
    pixels->clear();
    pixels->fill(color, i, fill_count);
    pixels->show();
    vTaskDelay(delay_ms / portTICK_PERIOD_MS);
  }
}

void Strip::fillReverse(int fill_count = 10) const {
  for (int i = max_leds - 1; i >= 0; i--) {
    pixels->clear();
    pixels->fill(color, i, fill_count);
    pixels->show();
    vTaskDelay(delay_ms / portTICK_PERIOD_MS);
  }
}

void Strip::stripTask() {
  pixels->clear();
  pixels->show();
  auto fill_count = 10;
  // a delay that will be applied to the end of each loop.
  for (;;) {
    if (pixels != nullptr) {
      if (status == StripStatus::FORWARD) {
        fillForward(fill_count);
      } else if (status == StripStatus::REVERSE) {
        fillReverse(fill_count);
      } else if (status == StripStatus::AUTO) {
        if (count % 2 == 0) {
          fillForward(fill_count);
        } else {
          fillReverse(fill_count);
        }
      } else if (status == StripStatus::STOP) {
        pixels->clear();
        pixels->show();
      }
    }
    const auto stop_halt_delay = 500;
    if (status == StripStatus::STOP) {
      vTaskDelay(stop_halt_delay / portTICK_PERIOD_MS);
    } else {
      vTaskDelay(halt_delay / portTICK_PERIOD_MS);
    }
    // after a round record the count and notify the client.
    if (pixels != nullptr && count_char != nullptr && status != StripStatus::STOP) {
      if (count < UINT32_MAX) {
        count++;
      } else {
        count = 0;
      }
      count_char->setValue(count);
      count_char->notify();
    }
  }
}

/**
 * @brief sets the maximum number of LEDs that can be used.
 * @param new_max_leds
 */
void Strip::setMaxLeds(int new_max_leds) {
  max_leds = new_max_leds;
  // delete already checks if the pointer is still pointing to a valid memory location.
  // If it is already nullptr, then it does nothing
  // free up the allocated memory from new
  delete pixels;
  // prevent dangling pointer
  pixels = nullptr;
  pixels = new Adafruit_NeoPixel(max_leds, pin, NEO_GRB + NEO_KHZ800);
  pixels->begin();
}

StripError Strip::initBLE(NimBLEServer *server) {
  if (server == nullptr) {
    return StripError::ERROR;
  }
  if(!is_ble_initialized){
    service = server->createService(LIGHT_SERVICE_UUID);
    color_char = service->createCharacteristic(LIGHT_CHAR_COLOR_UUID,
                                               NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto colorCharCallback = new ColorCharCallback(*this);
    color_char->setValue(color);
    color_char->setCallbacks(colorCharCallback);

    brightness_char = service->createCharacteristic(LIGHT_CHAR_BRIGHTNESS_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto brightnessCharCallback = new BrightnessCharCallback(*this);
    brightness_char->setValue(brightness);
    brightness_char->setCallbacks(brightnessCharCallback);

    delay_char = service->createCharacteristic(LIGHT_CHAR_DELAY_UUID,
                                               NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto delayCharCallback = new DelayCharCallback(*this);
    delay_char->setValue(delay_ms);
    delay_char->setCallbacks(delayCharCallback);

    max_leds_char = service->createCharacteristic(LIGHT_CHAR_MAX_LEDS_UUID,
                                                  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto maxLedsCharCallback = new MaxLEDsCharCallback(*this);
    max_leds_char->setValue(max_leds);
    max_leds_char->setCallbacks(maxLedsCharCallback);

    status_char = service->createCharacteristic(LIGHT_CHAR_STATUS_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto statusCharCallback = new StatusCharCallback(*this);
    status_char->setValue(status);
    status_char->setCallbacks(statusCharCallback);

    this->count_char = service->createCharacteristic(LIGHT_CHAR_COUNT_UUID,
                                                     NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    count_char->setValue(count);


    halt_delay_char = service->createCharacteristic(LIGHT_CHAR_HALT_DELAY_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto halt_cb = new HaltDelayCharCallback(*this);
    halt_delay_char->setValue(status);
    halt_delay_char->setCallbacks(halt_cb);

    service->start();
    is_ble_initialized = true;
    return StripError::OK;
  } else {
    return StripError::HAS_INITIALIZED;
  }
}

/**
 * @brief Get the instance/pointer of the strip.
 * @return
 */
Strip *Strip::get() {
  static auto *strip = new Strip();
  return strip;
}

/**
 * @brief initialize the strip. this function should only be called once.
 * @param max_leds
 * @param PIN the pin of strip, default is 14.
 * @param color the default color of the strip. default is Cyan (0x00FF00).
 * @param brightness the default brightness of the strip. default is 32.
 * @return StripError::OK if the strip is not inited, otherwise StripError::HAS_INITIALIZED.
 */
StripError Strip::begin(int max_leds, int16_t PIN, uint32_t color, uint8_t brightness) {
  if (!is_initialized){
    pref.begin("record", false);
    this->color = color;
    this->max_leds = max_leds;
    this->pin = PIN;
    this->brightness = brightness;
    pixels = new Adafruit_NeoPixel(max_leds, PIN, NEO_GRB + NEO_KHZ800);
    pixels->begin();
    pixels->setBrightness(brightness);
    is_initialized = true;
    return StripError::OK;
  } else {
    return StripError::HAS_INITIALIZED;
  }
}

void Strip::setBrightness(const uint8_t new_brightness) {
  if (pixels != nullptr) {
    brightness = new_brightness;
    pixels->setBrightness(brightness);
    if (brightness_char != nullptr) {
      brightness_char->setValue(brightness);
    }
  }
}

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

