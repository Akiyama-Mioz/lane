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

#define StripStatus_LENGTH 4
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
  int max_leds = 50;
  uint32_t count = 0;
  uint8_t brightness = 32;
  Adafruit_NeoPixel *pixels = nullptr;
  StripStatus status = StripStatus::AUTO;
  int delay_ms = 100;
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
  NimBLEService *service = nullptr;

  const char *LIGHT_SERVICE_UUID = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
  const char *LIGHT_CHAR_COLOR_UUID = "87a11e36-7c0e-44aa-a8ca-85307f52ce1e";
  const char *LIGHT_CHAR_BRIGHTNESS_UUID = "e3ce8b08-4bb9-4696-b862-3e62a1100adc";
  const char *LIGHT_CHAR_STATUS_UUID = "24207642-0d98-40cd-84bb-910008579114";
  const char *LIGHT_CHAR_MAX_LEDS_UUID = "28734897-4356-4c4a-af3d-8359ea4657cd";
  const char *LIGHT_CHAR_DELAY_UUID = "adbbac7f-2c08-4a8d-b3f7-d38d7bd5bc41";
  const char *LIGHT_CHAR_COUNT_UUID = "b972471a-139c-4211-a591-28c4ec1936f6";


  // you should call this in creatTask or something in RTOS.
  [[noreturn]]
  void stripTask();

  StripError initBLE(NimBLEServer *server);

  void setMaxLeds(int new_max_leds);

  void setBrightness(uint8_t new_brightness);

  void fillForward(int fill_count) const;

  void fillReverse(int fill_count) const;

  static Strip *get();

  Strip(Strip const &) = delete;

  Strip &operator=(Strip const &) = delete;

  Strip(Strip &&) = delete;

  Strip &operator=(Strip &&) = delete;

  // usually you won't destruct it because it's running in MCU.
  ~Strip() = delete;

  StripError
  begin(int max_leds, int16_t PIN, uint32_t color = Adafruit_NeoPixel::Color(255, 0, 255), uint8_t brightness = 32);

protected:
  Strip() = default;
};

// Maybe I should use factory pattern.
class ColorCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit ColorCharCallback(Strip &strip);
};

class BrightnessCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit BrightnessCharCallback(Strip &strip);
};

class DelayCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit DelayCharCallback(Strip &strip);
};

class MaxLEDsCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit MaxLEDsCharCallback(Strip &strip);
};

class StatusCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit StatusCharCallback(Strip &strip);
};


#endif //HELLO_WORLD_STRIP_H
