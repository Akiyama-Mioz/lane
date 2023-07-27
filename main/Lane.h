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
#include "pb_common.h"
#include "pb_decode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

namespace lane {
const auto PREF_RECORD_NAME    = "record";
const auto CIRCLE_LEDs_NUM_KEY = "c";
const auto CIRCLE_LENGTH_KEY   = "cl";
const auto TRACK_LEDs_NUM_KEY  = "t";

using centimeter = utils::length<float, std::centi>;
using meter      = utils::length<float, std::ratio<1>>;

// the line would be active for this length
const auto DEFAULT_ACTIVE_LENGTH = centimeter(60);
// line... it would wrap around
const auto DEFAULT_LINE_LENGTH = meter(50);
// like shift
const auto DEFAULT_TARGET_LENGTH = meter(1000);
const auto DEFAULT_LINE_LEDs_NUM = static_cast<uint32_t>(DEFAULT_LINE_LENGTH.count() * (100 / 3.3));
const auto DEFAULT_FPS           = 10;

static const auto BLUE_TRANSMIT_INTERVAL = std::chrono::milliseconds(1000);
static const auto HALT_INTERVAL          = std::chrono::milliseconds(500);
static const auto READY_INTERVAL         = std::chrono::milliseconds(500);
constexpr size_t DECODE_BUFFER_SIZE      = 2048;

enum class LaneStatus {
  FORWARD  = ::LaneStatus_FORWARD,
  BACKWARD = ::LaneStatus_BACKWARD,
  STOP     = ::LaneStatus_STOP,
};

std::string statusToStr(LaneStatus status);

enum class LaneError {
  OK = 0,
  ERROR,
  HAS_INITIALIZED,
};

struct LaneState {
  // scalar cumulative distance shift
  meter shift = meter(0);
  // m/s
  float speed = 0;
  // head should be always larger than tail
  meter head        = meter(0);
  meter tail        = meter(0);
  LaneStatus status = LaneStatus::STOP;
  static LaneState zero() {
    return LaneState{
        .shift  = meter(0),
        .speed  = 0,
        .head   = meter(0),
        .tail   = meter(0),
        .status = LaneStatus::STOP,
    };
  };
};

// won't change unless the status is STOP
struct LaneConfig {
  uint32_t color;
  meter line_length;
  meter active_length;
  meter total_length;
  uint32_t line_LEDs_num;
  float fps;
};

// input. could be changed by external device.
struct LaneParams {
  float speed;
  LaneStatus status;
};

LaneState nextState(LaneState last_state, LaneConfig cfg, LaneParams &input);

struct LaneBLE {
  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *ctrl_char   = nullptr;
  NimBLECharacteristic *config_char = nullptr;

  NimBLEService *service = nullptr;
};

//**************************************** Lane *********************************/

/**
 * @brief The Lane class
 */
class Lane {
public:
  [[nodiscard]] float getLEDsPerMeter() const;
  [[nodiscard]] auto getLaneLEDsNum() const {
    return this->cfg.line_LEDs_num;
  }

  Preferences pref;
  static const led_pixel_format_t LED_PIXEL_FORMAT = LED_PIXEL_FORMAT_RGB;
  int pin                                          = 23;

  /// in meter
  led_strip_handle_t led_strip                          = nullptr;
  std::array<uint8_t, DECODE_BUFFER_SIZE> decode_buffer = {0};
  LaneBLE ble                                           = {
                                                .ctrl_char   = nullptr,
                                                .config_char = nullptr,
                                                .service     = nullptr,
  };
  LaneConfig cfg = {
      .color         = utils::Colors::Red,
      .line_length   = DEFAULT_LINE_LENGTH,
      .active_length = utils::length_cast<meter>(DEFAULT_ACTIVE_LENGTH),
      .total_length  = DEFAULT_TARGET_LENGTH,
      .line_LEDs_num = DEFAULT_LINE_LEDs_NUM,
      .fps           = DEFAULT_FPS,
  };
  LaneState state   = LaneState::zero();
  LaneParams params = {
      .speed  = 0,
      .status = LaneStatus::STOP,
  };

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
   * @param [in,out]cfg mutate in place
   */
  void setMaxLEDs(uint32_t new_max_LEDs, LaneConfig &cfg);

  /**
   * @brief set the status of the strip.
   * @warning This function WILL set the corresponding bluetooth characteristic value and notify.
   * @param s
   */
  void notifyState(LaneState s);

  static Lane *get();

  Lane(Lane const &) = delete;

  Lane &operator=(Lane const &) = delete;

  Lane(Lane &&) = delete;

  Lane &operator=(Lane &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will not be released
  ~Lane() = delete;

  esp_err_t begin(int16_t PIN);

  /// set track length (count LEDs)
  void setCountLEDs(uint32_t count);

  void setCircleLength(float meter);

  void setSpeed(float speed) {
    this->params.speed = speed;
  };

  void setColor(uint32_t color) {
    this->cfg.color = color;
  };

  void setStatus(LaneStatus status) {
    this->params.status = status;
  };

  void setStatus(::LaneStatus status) {
    auto s              = static_cast<LaneStatus>(status);
    this->params.status = s;
  }

  inline void resetDecodeBuffer() {
    decode_buffer.fill(0);
  }

protected:
  bool is_initialized = false;
  Lane()              = default;

  void runOnce();

  void stop() const;
};

//*********************************** Callbacks ****************************************/

/**
 * @brief The ControlCharCallback class, which can notify the client the current state of the strip and accept the input from the client.
 */
class ControlCharCallback : public NimBLECharacteristicCallbacks {
  lane::Lane &lane;

public:
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override;

  explicit ControlCharCallback(lane::Lane &lane) : lane(lane){};
};

class ConfigCharCallback : public NimBLECharacteristicCallbacks {
  lane::Lane &lane;

public:
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override;

  explicit ConfigCharCallback(lane::Lane &lane) : lane(lane) {}
};
};

#endif // HELLO_WORLD_STRIP_H
