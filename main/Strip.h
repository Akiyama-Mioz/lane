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
#include "map"
using namespace std;
std::map<uint,uint> speed_male;   //cm/10s    

std::map<uint,uint> speed_female;

uint speed800[3][8]  ={{3968,3968,3663,3968,3401,3663,3401,4762},  //100  score
                        {3805,3805,3513,3805,3262,3513,3262,4566},  //90 score
                        {3546,3546,3273,3546,3040,3273,304,4255,}}; //80 score
uint speed1000[3][10]={{5107,4546,4546,4546,4546,4591,4289,4289,4546,4546},
                        {4885,4348,4348,4348,4348,4392,4102,4102,4348,4348},
                        {4586,4082,4082,4082,4082,4123,3851,3851,4082,4082}}; 
// change this to match the length of StripStatus
constexpr uint8_t StripStatus_LENGTH = 4;
enum class StripStatus {
  AUTO = 0,
  RUN800,
  RUN1000,
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
  // 10 LEDs/m for 24v
  int max_LEDs = 4000;
  // length should be less than max_LEDs
  uint16_t length = 10;
  uint32_t count = 0;
  uint8_t brightness = 32;
  Adafruit_NeoPixel *pixels = nullptr;
  StripStatus status = StripStatus::AUTO;
  int delay_ms = 100;
  int halt_delay = 500;
  uint32_t color[3] ={ Adafruit_NeoPixel::Color(255, 0, 255),
                      Adafruit_NeoPixel::Color(255, 255,0),
                      Adafruit_NeoPixel::Color(0, 255, 255)};

  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *shift_char = nullptr;
  NimBLECharacteristic *color_char = nullptr;
  NimBLECharacteristic *status_char = nullptr;
  NimBLECharacteristic *brightness_char = nullptr;
  NimBLECharacteristic *max_leds_char = nullptr;
  NimBLECharacteristic *delay_char = nullptr;
  NimBLECharacteristic *halt_delay_char = nullptr;
  NimBLECharacteristic *speed_char = nullptr;
  NimBLEService *service = nullptr;

  const char *LIGHT_SERVICE_UUID = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
  const char *LIGHT_CHAR_COLOR_UUID = "87a11e36-7c0e-44aa-a8ca-85307f52ce1e";
  const char *LIGHT_CHAR_BRIGHTNESS_UUID = "e3ce8b08-4bb9-4696-b862-3e62a1100adc";
  const char *LIGHT_CHAR_STATUS_UUID = "24207642-0d98-40cd-84bb-910008579114";
  const char *LIGHT_CHAR_MAX_LEDS_UUID = "28734897-4356-4c4a-af3d-8359ea4657cd";
  const char *LIGHT_CHAR_DELAY_UUID = "adbbac7f-2c08-4a8d-b3f7-d38d7bd5bc41";
  const char *LIGHT_CHAR_SHIFT_UUID = "b972471a-139c-4211-a591-28c4ec1936f6";
  const char *LIGHT_CHAR_HALT_DELAY_UUID = "00923454-81de-4e74-b2e0-a873f2cbddcc";
  const char *LIGHT_CHAR_SPEED_UUID = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
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

 void RUN800() const;

 void Get_color();

  void Get_speed();

  void RUN1000() const;

  static Strip *get();

  Strip(Strip const &) = delete;

  Strip &operator=(Strip const &) = delete;

  Strip(Strip &&) = delete;

  Strip &operator=(Strip &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will be released
  ~Strip() = delete;

  StripError
  begin(int max_LEDs, int16_t PIN ,uint8_t brightness = 32);

protected:
  Strip() = default;
};


#endif //HELLO_WORLD_STRIP_H
