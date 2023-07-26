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
#include "lane.pb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

const auto LANE_PREF_RECORD_NAME    = "record";
const auto LANE_BRIGHTNESS_KEY      = "b";
const auto LANE_CIRCLE_LEDs_NUM_KEY = "c";
const auto LANE_CIRCLE_LENGTH_KEY   = "cl";
const auto LANE_TRACK_LEDs_NUM_KEY  = "t";

/// in count
const uint32_t LANE_DEFAULT_CIRCLE_LEDs_NUM = 3050;
/// in count
const uint32_t LANE_DEFAULT_TRACK_LEDs_NUM = 48;

// 6m
const auto LANE_DEFAULT_LINE_LENGTH = utils::centimeter(static_cast<int>(600));

// in ms
static const auto BLUE_TRANSMIT_INTERVAL    = std::chrono::milliseconds(1000);
static const auto HALT_INTERVAL             = std::chrono::milliseconds(500);
static const auto READY_INTERVAL            = std::chrono::milliseconds(500);
constexpr size_t LANE_DECODE_BUFFER_SIZE   = 2048;

namespace lane {
enum class LaneStatus {
  FORWARD = ::LaneStatus_FORWARD,
  BACKWARD = ::LaneStatus_BACKWARD,
  STOP = ::LaneStatus_STOP,
};

/**
 * @brief convert LaneStatus to int that meaningful to the lane
 * @param status
 * @return 1 for forward, -1 for backward, 0 for stop
 */
int laneStatusToInt(LaneStatus status){
  switch (status) {
    case LaneStatus::FORWARD:
      return 1;
    case LaneStatus::BACKWARD:
      return -1;
    case LaneStatus::STOP:
      return 0;
  }
};

enum class LaneError {
  OK = 0,
  ERROR,
  HAS_INITIALIZED,
};

struct RunState {
  float position;
  // m/s
  float speed;
  // in meter
  // scalar cumulative distance shift
  float shift;
};

RunState
nextState(RunState state, const ValueRetriever<float> &retriever, float circleLength, float trackLength, float fps);

struct LaneBLE {
  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *ctrl_char   = nullptr;
  NimBLECharacteristic *config_char = nullptr;

  NimBLEService *service = nullptr;
};

class Lane {
protected:
  bool is_initialized = false;

public:
  float getLEDsPerMeter() const;
  auto getLaneLEDsNum() const {
    return max_LEDs;
  }

  // it takes 90ms for 3000 LEDs so 10 it should be okay at 10 FPS
  constexpr static const float FPS                 = 10;
  static const led_pixel_format_t LED_PIXEL_FORMAT = LED_PIXEL_FORMAT_RGB;
  Preferences pref;
  int pin           = 14;
  uint32_t max_LEDs = 0;
  /// the LED count that is filled at once per track and should be less than `max_LEDs`
  uint32_t count_LEDs = 10;
  /// in meter
  utils::centimeter circle_length = LANE_DEFAULT_LINE_LENGTH;

  led_strip_handle_t led_strip = nullptr;
  std::array<uint8_t, LANE_DECODE_BUFFER_SIZE> decode_buffer = {0};
  LaneBLE ble;

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
   * @brief set the status of the strip.
   * @warning This function WILL set the corresponding bluetooth characteristic value and notify.
   * @param s
   */
  void setStatusNotify(LaneStatus s);

  static Lane *get();

  Lane(Lane const &) = delete;

  Lane &operator=(Lane const &) = delete;

  Lane(Lane &&) = delete;

  Lane &operator=(Lane &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will not be released
  ~Lane() = delete;

  LaneError
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
};

#endif // HELLO_WORLD_STRIP_H
