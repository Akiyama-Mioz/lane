//
// Created by Kurosu Chan on 2022/7/13.
//

#ifndef HELLO_WORLD_STRIP_H
#define HELLO_WORLD_STRIP_H

#include "esp_random.h"
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

const auto STRIP_PREF_RECORD_NAME = "record";
const auto STRIP_BRIGHTNESS_KEY = "b";
const auto STRIP_CIRCLE_LEDs_NUM_KEY = "c";
const auto STRIP_CIRCLE_LENGTH_KEY = "cl";
const auto STRIP_TRACK_LEDs_NUM_KEY = "t";

// STRIP_DEFAULT_LEDs_PER_METER is calculated
/// in count
const uint32_t STRIP_DEFAULT_CIRCLE_LEDs_NUM = 3050;
/// in count
const uint32_t STRIP_DEFAULT_TRACK_LEDs_NUM = 48;
// in meter
const uint32_t STRIP_DEFAULT_CIRCLE_LENGTH = 400;
// in ms
static const int BLUE_TRANSMIT_INTERVAL_MS = 1000;
// in ms
static const int HALT_INTERVAL = 100;

enum class StripError {
  OK = 0,
  ERROR,
  HAS_INITIALIZED,
};

struct RunState {
  float position;
  float speed;
  float shift;
  float extra;
};

RunState
nextState(RunState state, const ValueRetriever<float> &retriever, float circleLength, float trackLength, float fps);

class Track {
public:
  int32_t id = 0;
  RunState state = RunState{0, 0, 0, false};
  ValueRetriever<float> retriever = ValueRetriever<float>(std::map<int, float>());
  uint32_t color = Adafruit_NeoPixel::Color(255, 255, 255);

  [[nodiscard]]
  int getMaxShiftLength() const {
    return retriever.getMaxKey();
  }

  void resetState() {
    this->state = RunState{0, 0, 0, false};
  }

  RunState
  updateStrip(Adafruit_NeoPixel *pixels, float circle_length, float track_length, float fps, float LEDs_per_meter);
};

class Strip {
protected:
  bool is_initialized = false;
  bool is_ble_initialized = false;
public:
  float getLEDsPerMeter() const;

  // it takes 90ms for 3000 LEDs so 10 it should be okay at 10 FPS
  constexpr static const float FPS = 10;
  static const neoPixelType pixel_type = NEO_RGB + NEO_KHZ800;
  Preferences pref;
  int pin = 14;
  uint32_t max_LEDs = 0;
  /// the LED count that is filled at once per track and should be less than `max_LEDs`
  uint32_t count_LEDs = 10;
  uint8_t brightness = 32;
  /// in meter
  float circle_length = STRIP_DEFAULT_CIRCLE_LENGTH;
  Adafruit_NeoPixel *pixels = nullptr;
  TrackStatus status = TrackStatus_STOP;

  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *status_char = nullptr;
  NimBLECharacteristic *brightness_char = nullptr;
  NimBLECharacteristic *options_char = nullptr;
  NimBLECharacteristic *config_char = nullptr;
  NimBLECharacteristic *state_char = nullptr;

  NimBLEService *service = nullptr;
  const char *LIGHT_SERVICE_UUID = "15ce51cd-7f34-4a66-9187-37d30d3a1464";

  const char *LIGHT_CHAR_BRIGHTNESS_UUID = "e3ce8b08-4bb9-4696-b862-3e62a1100adc";
  const char *LIGHT_CHAR_STATUS_UUID = "24207642-0d98-40cd-84bb-910008579114";
  const char *LIGHT_CHAR_OPTIONS_CHAR = "9f5806ba-a71b-4194-9854-5d76698200de";
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
   * @brief sets the maximum number of LEDs that can be used. i.e. Circle Length.
   * @warning This function will NOT set the corresponding bluetooth characteristic value.
   * @param new_max_LEDs
   */
  void setMaxLEDs(uint32_t new_max_LEDs);

/**
 * @brief set the color of the strip.
 * @warning This function will NOT set the corresponding bluetooth characteristic value.
 * @param color the color of the strip.
 */
  void setBrightness(uint8_t new_brightness);

  /**
   * @brief set the status of the strip.
   * @warning This function WILL set the corresponding bluetooth characteristic value and notify.
   * @param s
   */
  void setStatusNotify(TrackStatus s);

  static Strip *get();

  Strip(Strip const &) = delete;

  Strip &operator=(Strip const &) = delete;

  Strip(Strip &&) = delete;

  Strip &operator=(Strip &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will not be released
  ~Strip() = delete;

  StripError
  begin(int16_t PIN, uint8_t brightness);

  /// set track length (count LEDs)
  void setCountLEDs(uint32_t count);

  void setCircleLength(float meter);

protected:
  Strip() = default;

  void run(std::vector<Track> &tracks);
};


#endif //HELLO_WORLD_STRIP_H
