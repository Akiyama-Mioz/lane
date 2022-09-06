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
#include "track_config.pb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// in meters
static const int CIRCLE_LENGTH = 400;
static const int LEDs_PER_METER = 10;
// in ms
static const int TRANSMIT_INTERVAL = 1000;
// in ms
static const int HALT_INTERVAL = 100;

// change this to match the length of StripStatus
constexpr uint8_t StripStatus_LENGTH = 2;
enum class StripStatus {
  STOP = 0,
  RUN,
};

enum class StripError {
  OK = 0,
  ERROR,
  HAS_INITIALIZED,
};

struct RunState {
  float position;
  float speed;
  float shift;
  bool isSkip;
};

RunState nextState(RunState state, const ValueRetriever<float> &retriever, int ledCounts, float fps);

class Track {
public:
  int32_t id = 0;
  RunState state = RunState{0, 0, 0, false};
  ValueRetriever<float> retriever = ValueRetriever<float>(std::map<int, float>());
  uint32_t color = Adafruit_NeoPixel::Color(255, 255, 255);

  [[nodiscard]]
  int getMaxLength() const {
    return retriever.getMaxKey();
  }

  void resetState() {
    this->state = RunState{0, 0, 0, false};
  }

  RunState updateStrip(Adafruit_NeoPixel *pixels, int ledCounts, float fps);
};

class Strip {
protected:
  bool is_initialized = false;
  bool is_ble_initialized = false;
public:
  //distance travelled. MAX = 7
  constexpr static const float fps = 6;
  static const neoPixelType pixelType = NEO_RGB + NEO_KHZ800;
  Preferences pref;
  int pin = 14;
  // 10 LEDs/m for 24V version
  int max_LEDs = 0;
  // length should be less than max_LEDs
  // the LED count that is filled once
  uint16_t countLEDs = 10;
  uint8_t brightness = 32;
  Adafruit_NeoPixel *pixels = nullptr;
  StripStatus status = StripStatus::STOP;

  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *status_char = nullptr;
  NimBLECharacteristic *brightness_char = nullptr;
  NimBLECharacteristic *config_char = nullptr;
  NimBLECharacteristic *state_char = nullptr;

  NimBLEService *service = nullptr;
  const char *LIGHT_SERVICE_UUID = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
  const char *LIGHT_CHAR_BRIGHTNESS_UUID = "e3ce8b08-4bb9-4696-b862-3e62a1100adc";
  const char *LIGHT_CHAR_STATUS_UUID = "24207642-0d98-40cd-84bb-910008579114";

  const char *LIGHT_CHAR_CONFIG_UUID = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
  const char *LIGHT_CHAR_STATE_UUID = "ed3eefa1-3c80-b43f-6b65-e652374650b5";

  std::vector<Track> tracks = std::vector<Track>{};

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

  /**
   * @brief sets the maximum number of LEDs that can be used.
   * @warning This function will NOT set the corresponding bluetooth characteristic value.
   * @param new_max_LEDs
   */
  void setMaxLEDs(int new_max_LEDs);

/**
 * @brief set the color of the strip.
 * @warning This function will NOT set the corresponding bluetooth characteristic value.
 * @param color the color of the strip.
 */
  void setBrightness(uint8_t new_brightness);

  /**
   * @brief set the status of the strip.
   * @warning This function WILL set the corresponding bluetooth characteristic value and notify.
   * @param status
   */
  void setStatusNotify(StripStatus status);

  static Strip *get();

  Strip(Strip const &) = delete;

  Strip &operator=(Strip const &) = delete;

  Strip(Strip &&) = delete;

  Strip &operator=(Strip &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will not be released
  ~Strip() = delete;

  StripError
  begin(int16_t PIN, uint8_t brightness);

protected:
  Strip() = default;

  void run(std::vector<Track> &tracks);
};


#endif //HELLO_WORLD_STRIP_H
