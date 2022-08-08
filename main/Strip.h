//
// Created by Kurosu Chan on 2022/7/13.
//

#ifndef HELLO_WORLD_STRIP_H
#define HELLO_WORLD_STRIP_H

#include <Adafruit_NeoPixel.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "NimBLEDevice.h"
#include "Preferences.h"

// change this to match the length of StripStatus
constexpr uint8_t StripStatus_LENGTH = 4;
enum class StripStatus {
  AUTO = 0,
  FORWARD,
  REVERSE,
  STOP,
};

enum class StripError {
  OK = 0,
  ERROR,
  HAS_INITIALIZED,
};

class Strip {
protected:
  bool is_initialized = false;
  bool is_ble_initialized = false;
public:
  Preferences pref;
  int pin = 14;
  // 10 LEDs/m foo 24v
  int max_LEDs = 50;
  // length should be less than max_LEDs
  uint16_t length = 10;
  uint32_t count = 0;
  uint8_t brightness = 32;
  Adafruit_NeoPixel *pixels = nullptr;
  StripStatus status = StripStatus::AUTO;
  int delay_ms = 100;
  int halt_delay = 500;
  uint32_t color = Adafruit_NeoPixel::Color(255, 0, 255);
  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *count_char = nullptr;
  NimBLECharacteristic *color_char = nullptr;
  NimBLECharacteristic *status_char = nullptr;
  NimBLECharacteristic *brightness_char = nullptr;
  NimBLECharacteristic *max_leds_char = nullptr;
  NimBLECharacteristic *delay_char = nullptr;
  NimBLECharacteristic *halt_delay_char = nullptr;
  NimBLEService *service = nullptr;

  const char *LIGHT_SERVICE_UUID = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
  const char *LIGHT_CHAR_COLOR_UUID = "87a11e36-7c0e-44aa-a8ca-85307f52ce1e";
  const char *LIGHT_CHAR_BRIGHTNESS_UUID = "e3ce8b08-4bb9-4696-b862-3e62a1100adc";
  const char *LIGHT_CHAR_STATUS_UUID = "24207642-0d98-40cd-84bb-910008579114";
  const char *LIGHT_CHAR_MAX_LEDS_UUID = "28734897-4356-4c4a-af3d-8359ea4657cd";
  const char *LIGHT_CHAR_DELAY_UUID = "adbbac7f-2c08-4a8d-b3f7-d38d7bd5bc41";
  const char *LIGHT_CHAR_COUNT_UUID = "b972471a-139c-4211-a591-28c4ec1936f6";
  const char *LIGHT_CHAR_HALT_DELAY_UUID = "00923454-81de-4e74-b2e0-a873f2cbddcc";


  /**
   * @brief Loop the strip.
   * @warning This function will never return and you should call this in creatTask/Thread
   *          or something equivalent in RTOS.
   * @param void void
   * @return No return
   */
  [[noreturn]]
  void stripTask();

  StripError initBLE(NimBLEServer *server);

  void setMaxLEDs(int new_max_LEDs);

  void setBrightness(uint8_t new_brightness);

  void fillForward() const;

  void fillReverse() const;

  static Strip *get();

  Strip(Strip const &) = delete;

  Strip &operator=(Strip const &) = delete;

  Strip(Strip &&) = delete;

  Strip &operator=(Strip &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will be released
  ~Strip() = delete;

  StripError
  begin(int max_LEDs, int16_t PIN, uint32_t color = Adafruit_NeoPixel::Color(255, 0, 255), uint8_t brightness = 32);

protected:
  Strip() = default;
};


#endif //HELLO_WORLD_STRIP_H
