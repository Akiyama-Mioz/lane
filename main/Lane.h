//
// Created by Kurosu Chan on 2022/7/13.
//

#ifndef HELLO_WORLD_STRIP_H
#define HELLO_WORLD_STRIP_H

#include "esp_random.h"
#include "utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "NimBLEDevice.h"
#include "Preferences.h"
#include "led_strip.h"
#include "map"
#include "vector"
#include "track_config.pb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

const auto STRIP_PREF_RECORD_NAME    = "record";
const auto STRIP_BRIGHTNESS_KEY      = "b";
const auto STRIP_CIRCLE_LEDs_NUM_KEY = "c";
const auto STRIP_CIRCLE_LENGTH_KEY   = "cl";
const auto STRIP_TRACK_LEDs_NUM_KEY  = "t";

// STRIP_DEFAULT_LEDs_PER_METER is calculated
/// in count
const uint32_t STRIP_DEFAULT_CIRCLE_LEDs_NUM = 3050;
/// in count
const uint32_t STRIP_DEFAULT_TRACK_LEDs_NUM = 48;
// in meter
const uint32_t STRIP_DEFAULT_FULL_LINE_LENGTH = 50;
// in ms
static const int BLUE_TRANSMIT_INTERVAL_MS = 1000;
static const int HALT_INTERVAL_MS          = 100;
static const int READY_INTERVAL_MS         = 500;
constexpr size_t STRIP_DECODE_BUFFER_SIZE  = 2048;

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
  int32_t id                      = 0;
  RunState state                  = RunState{0, 0, 0, false};
  ValueRetriever<float> retriever = ValueRetriever<float>(std::map<int, float>());
  uint32_t color                  = Adafruit_NeoPixel::Color(255, 255, 255);

  [[nodiscard]] int getMaxShiftLength() const {
    return retriever.getMaxKey();
  }

  void resetState() {
    this->state = RunState{0, 0, 0, false};
  }

  RunState
  updateStrip(led_strip_handle_t handle, float circle_length, float track_length, float fps, float LEDs_per_meter);
};

struct LaneBLE {
  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *ctrl_char       = nullptr;
  NimBLECharacteristic *config_char     = nullptr;

  NimBLEService *service         = nullptr;
};

class Lane {
protected:
  bool is_initialized     = false;

public:
  float getLEDsPerMeter() const;
  auto getCircleLEDsNum() const {
    return max_LEDs;
  }

  // it takes 90ms for 3000 LEDs so 10 it should be okay at 10 FPS
  constexpr static const float FPS = 10;
  // static const neoPixelType pixel_type = NEO_RGB + NEO_KHZ800;
  static const led_pixel_format_t LED_PIXEL_FORMAT = LED_PIXEL_FORMAT_RGB;
  Preferences pref;
  int pin           = 14;
  uint32_t max_LEDs = 0;
  /// the LED count that is filled at once per track and should be less than `max_LEDs`
  uint32_t count_LEDs = 10;
  uint8_t brightness  = 32;
  /// in meter
  float circle_length = STRIP_DEFAULT_FULL_LINE_LENGTH;

  led_strip_handle_t led_strip                                = nullptr;
  TrackStatus status                                          = TrackStatus_STOP;
  std::array<uint8_t, STRIP_DECODE_BUFFER_SIZE> decode_buffer = {0};

  LaneBLE ble;
  std::vector<Track> tracks = std::vector<Track>{};

  /**
   * @brief Loop the strip.
   * @warning This function will never return and you should call this in creatTask/Thread
   *          or something equivalent in RTOS.
   * @param void void
   * @return No return
   */
  [[noreturn]] void loop();

  void setBLE(LaneBLE ble) {
    this->ble = ble;
  };

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

  static Lane *get();

  Lane(Lane const &) = delete;

  Lane &operator=(Lane const &) = delete;

  Lane(Lane &&) = delete;

  Lane &operator=(Lane &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will not be released
  ~Lane() = delete;

  StripError
  begin(int16_t PIN, uint8_t brightness);

  /// set track length (count LEDs)
  void setCountLEDs(uint32_t count);

  void setCircleLength(float meter);

  inline void resetDecodeBuffer() {
    decode_buffer.fill(0);
  }

protected:
  Lane() = default;

  void run(std::vector<Track> &tracks);

  void ready(Instant &last_blink) const;

  void stop() const;
};

#endif // HELLO_WORLD_STRIP_H
