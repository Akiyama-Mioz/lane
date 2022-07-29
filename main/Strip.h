//
// Created by Kurosu Chan on 2022/7/13.
//

#ifndef HELLO_WORLD_STRIP_H
#define HELLO_WORLD_STRIP_H

#include <Adafruit_NeoPixel.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "NimBLEDevice.h"

enum class StripStatus {
  AUTO = 0,
  FORWARD,
  REVERSE,
  STOP,
};

class Strip {
public:
//const int fps = 10;  // refresh each 100ms
// 14
  int pin;
// 24v:10 leds/m
// 45 for 2 meters.
  int max_leds;
  int delay_ms = 500;
  uint32_t count = 0;
  uint8_t brightness = 32;
  uint32_t color = Adafruit_NeoPixel::Color(255, 0, 255);
  Adafruit_NeoPixel *pixels;
  StripStatus status = StripStatus::AUTO;
  NimBLECharacteristic * count_char = nullptr;

  const char *LIGHT_SERVICE_UUID = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
  const char *LIGHT_CHAR_COLOR_UUID = "87a11e36-7c0e-44aa-a8ca-85307f52ce1e";
  const char *LIGHT_CHAR_BRIGHTNESS_UUID = "e3ce8b08-4bb9-4696-b862-3e62a1100adc";
  const char *LIGHT_CHAR_STATUS_UUID = "24207642-0d98-40cd-84bb-910008579114";
  const char *LIGHT_CHAR_LENGTH_UUID = "28734897-4356-4c4a-af3d-8359ea4657cd";
  const char *LIGHT_CHAR_DELAY_UUID = "adbbac7f-2c08-4a8d-b3f7-d38d7bd5bc41";
  const char *LIGHT_CHAR_COUNT_UUID = "b972471a-139c-4211-a591-28c4ec1936f6";

  Strip(int max_leds, int PIN);

  [[noreturn]]
  void stripTask();
  void registerBLE(NimBLEServer *server);
  void updateMaxLength(int new_max_leds);

  void fillForward(int fill_count) const;

  void fillReverse(int fill_count) const;
};

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

class LengthCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;
  explicit LengthCharCallback(Strip &strip);
};

class StatusCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;
  explicit StatusCharCallback(Strip &strip);
};


#endif //HELLO_WORLD_STRIP_H
