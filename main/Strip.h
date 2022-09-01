//
// Created by Kurosu Chan on 2022/7/13.
//

#ifndef HELLO_WORLD_STRIP_H
#define HELLO_WORLD_STRIP_H

#include "utils.h"
#include <Adafruit_NeoPixel.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "NimBLEDevice.h"
#include "Preferences.h"
#include "map"
#include "vector"

static auto initDists = std::vector<int>{100, 200, 300, 400, 500, 600, 700, 800};
static auto initSpeed0 = std::vector<int>{3968, 3968, 3663, 3968, 3401, 3663, 3401, 4762};
static auto initSpeed1 = std::vector<int>{3805, 3805, 3513, 3805, 3262, 3513, 3262, 4566};
static auto initSpeed2 = std::vector<int>{3546, 3546, 3273, 3546, 3040, 3273, 304, 4255,};
static auto initSpeedCustom = std::vector<int>{3968, 3968, 3663, 3968, 3401, 3663, 3401, 4762};
// change this to match the length of StripStatus
constexpr uint8_t StripStatus_LENGTH = 4;
enum class StripStatus {
  STOP = 0,
  NORMAL,
  CUSTOM
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
  float fps = 6; //distance travelled ! max = 7 !
  bool isStop = 0;
  Preferences pref;
  int pin = 14;
  // 10 LEDs/m for 24v
  int max_LEDs = 4000;
  // length should be less than max_LEDs
  uint16_t length = 10;
  uint32_t count = 0;
  uint8_t brightness = 32;
  Adafruit_NeoPixel *pixels = nullptr;
  StripStatus status = StripStatus::STOP;
  int delay_ms = 100;
  int halt_delay = 500;
  uint32_t color[3] = {Adafruit_NeoPixel::Color(255, 0, 255),
                       Adafruit_NeoPixel::Color(255, 255, 0),
                       Adafruit_NeoPixel::Color(0, 255, 255)};

  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *shift_char = nullptr;
  NimBLECharacteristic *color_char = nullptr;
  NimBLECharacteristic *status_char = nullptr;
  NimBLECharacteristic *brightness_char = nullptr;
  NimBLECharacteristic *max_LEDs_char = nullptr;
  NimBLECharacteristic *delay_char = nullptr;
  NimBLECharacteristic *halt_delay_char = nullptr;
  NimBLECharacteristic *speed_custom_char = nullptr;
  NimBLECharacteristic *speed0_char = nullptr;
  NimBLECharacteristic *speed1_char = nullptr;
  NimBLECharacteristic *speed2_char = nullptr;

  NimBLEService *service = nullptr;
  const char *LIGHT_SERVICE_UUID = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
  const char *LIGHT_CHAR_COLOR_UUID = "87a11e36-7c0e-44aa-a8ca-85307f52ce1e";
  const char *LIGHT_CHAR_BRIGHTNESS_UUID = "e3ce8b08-4bb9-4696-b862-3e62a1100adc";
  const char *LIGHT_CHAR_STATUS_UUID = "24207642-0d98-40cd-84bb-910008579114";
  const char *LIGHT_CHAR_MAX_LEDs_UUID = "28734897-4356-4c4a-af3d-8359ea4657cd";
  const char *LIGHT_CHAR_DELAY_UUID = "adbbac7f-2c08-4a8d-b3f7-d38d7bd5bc41";
  const char *LIGHT_CHAR_SHIFT_UUID = "b972471a-139c-4211-a591-28c4ec1936f6";
  const char *LIGHT_CHAR_HALT_DELAY_UUID = "00923454-81de-4e74-b2e0-a873f2cbddcc";
  const char *LIGHT_CHAR_SPEED0_UUID = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
  const char *LIGHT_CHAR_SPEED1_UUID = "ed3eefa1-3c80-b43f-6b65-e652374650b5";
  const char *LIGHT_CHAR_SPEED2_UUID = "765961ad-e273-ddda-ce56-25a46e3017f9";
  const char *LIGHT_CHAR_SPEED_CUSTOM_UUID = "765961ad-e273-ddda-ce56-25a46e3017f9";
  std::vector<ValueRetriever<float>> speedVals = std::vector(3,ValueRetriever<float>(std::map<int, float>()));
  ValueRetriever<float> speedCustom = ValueRetriever<float>(std::map<int, float>());

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

  void runCustom();

  void getColor();

  void runNormal();

  static Strip *get();

  Strip(Strip const &) = delete;

  Strip &operator=(Strip const &) = delete;

  Strip(Strip &&) = delete;

  Strip &operator=(Strip &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will be released
  ~Strip() = delete;

  StripError
  begin(int max_LEDs, int16_t PIN, uint8_t brightness = 32);

protected:
  Strip() = default;
};


#endif //HELLO_WORLD_STRIP_H
