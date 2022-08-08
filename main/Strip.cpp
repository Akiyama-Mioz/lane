//
// Created by Kurosu Chan on 2022/7/13.
//
#include "StripCommon.h"

void Strip::fillForward() const {
  for (int i = 0; i < max_LEDs; i++) {
    pixels->clear();
    pixels->fill(color, i, length);
    pixels->show();
    vTaskDelay(delay_ms / portTICK_PERIOD_MS);
  }
}

void Strip::fillReverse() const {
  for (int i = max_LEDs - length; i >= 0; i--) {
    pixels->clear();
    pixels->fill(color, i, length);
    pixels->show();
    vTaskDelay(delay_ms / portTICK_PERIOD_MS);
  }
}

void Strip::stripTask() {
  pixels->clear();
  pixels->show();
  // a delay that will be applied to the end of each loop.
  for (;;) {
    if (pixels != nullptr) {
      if (status == StripStatus::FORWARD) {
        fillForward();
      } else if (status == StripStatus::REVERSE) {
        fillReverse();
      } else if (status == StripStatus::AUTO) {
        if (count % 2 == 0) {
          fillForward();
        } else {
          fillReverse();
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
 * @warning This function will not set the corresponding bluetooth characteristic value.
 * @param new_max_LEDs
 */
void Strip::setMaxLEDs(int new_max_LEDs) {
  max_LEDs = new_max_LEDs;
  // delete already checks if the pointer is still pointing to a valid memory location.
  // If it is already nullptr, then it does nothing
  // free up the allocated memory from new
  delete pixels;
  // prevent dangling pointer
  pixels = nullptr;
  /**
   * @note Adafruit_NeoPixel::updateLength(uint16_t n) has been deprecated and only for old projects that
   *       may still be calling it. New projects should instead use the
   *       'new' keyword with the first constructor syntax (length, pin,
   *       type).
   */
  pixels = new Adafruit_NeoPixel(max_LEDs, pin, NEO_GRB + NEO_KHZ800);
  pixels->setBrightness(brightness);
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
    auto color_cb = new ColorCharCallback(*this);
    color_char->setValue(color);
    color_char->setCallbacks(color_cb);

    brightness_char = service->createCharacteristic(LIGHT_CHAR_BRIGHTNESS_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto brightness_cb = new BrightnessCharCallback(*this);
    brightness_char->setValue(brightness);
    brightness_char->setCallbacks(brightness_cb);

    delay_char = service->createCharacteristic(LIGHT_CHAR_DELAY_UUID,
                                               NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto delay_cb = new DelayCharCallback(*this);
    delay_char->setValue(delay_ms);
    delay_char->setCallbacks(delay_cb);

    max_leds_char = service->createCharacteristic(LIGHT_CHAR_MAX_LEDS_UUID,
                                                  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto max_LEDs_cb = new MaxLEDsCharCallback(*this);
    max_leds_char->setValue(max_LEDs);
    max_leds_char->setCallbacks(max_LEDs_cb);

    status_char = service->createCharacteristic(LIGHT_CHAR_STATUS_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto status_cb = new StatusCharCallback(*this);
    status_char->setValue(status);
    status_char->setCallbacks(status_cb);

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
 * @return the instance/pointer of the strip.
 */
Strip *Strip::get() {
  static auto *strip = new Strip();
  return strip;
}

/**
 * @brief initialize the strip. this function should only be called once.
 * @param max_LEDs
 * @param PIN the pin of strip, default is 14.
 * @param color the default color of the strip. default is Cyan (0x00FF00).
 * @param brightness the default brightness of the strip. default is 32.
 * @return StripError::OK if the strip is not inited, otherwise StripError::HAS_INITIALIZED.
 */
StripError Strip::begin(int max_LEDs, int16_t PIN, uint32_t color, uint8_t brightness) {
  if (!is_initialized){
    pref.begin("record", false);
    this->color = color;
    this->max_LEDs = max_LEDs;
    this->pin = PIN;
    this->brightness = brightness;
    pixels = new Adafruit_NeoPixel(max_LEDs, PIN, NEO_GRB + NEO_KHZ800);
    pixels->begin();
    pixels->setBrightness(brightness);
    is_initialized = true;
    return StripError::OK;
  } else {
    return StripError::HAS_INITIALIZED;
  }
}

/**
 * @brief set the color of the strip.
 * @warning This function will not set the corresponding bluetooth characteristic value.
 * @param color the color of the strip.
 */
void Strip::setBrightness(const uint8_t new_brightness) {
  if (pixels != nullptr) {
    brightness = new_brightness;
    pixels->setBrightness(brightness);
  }
}
