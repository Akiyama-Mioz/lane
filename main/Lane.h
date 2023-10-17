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
#include "map"
#include "vector"
#include "lane.pb.h"
#include "pb_common.h"
#include "pb_decode.h"
#include "Strip.hpp"
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Adafruit_NeoPixel.h"

namespace lane {
const auto PREF_RECORD_NAME = "rec";
// float
const auto PREF_LINE_LENGTH_NAME = "ll";
// float
const auto PREF_ACTIVE_LENGTH_NAME = "al";
// uint32_t
const auto PREF_LINE_LEDs_NUM_NAME = "ln";
// float
const auto PREF_TOTAL_LENGTH_NAME = "to";
// uint32_t
const auto PREF_COLOR_NAME = "co";

const auto PREF_WHITE_LIST_NAME = "wh";
const auto PREF_WHITE_LIST_MAX_LENGTH = 216;

using centimeter = utils::length<float, std::centi>;
using meter      = utils::length<float, std::ratio<1>>;

// the line would be active for this length
const auto DEFAULT_ACTIVE_LENGTH = meter(0.6);
// line... it would wrap around
const auto DEFAULT_LINE_LENGTH = meter(50);
// like shift
const auto DEFAULT_TARGET_LENGTH = meter(1000);
const auto DEFAULT_LINE_LEDs_NUM = static_cast<uint32_t>(DEFAULT_LINE_LENGTH.count() * (100 / 3.3));
const auto DEFAULT_FPS           = 10;

static const auto BLUE_TRANSMIT_INTERVAL = std::chrono::milliseconds(1000);
static const auto HALT_INTERVAL          = std::chrono::milliseconds(500);
static const auto READY_INTERVAL         = std::chrono::milliseconds(500);
// mem_block_symbols must be even and at least 64
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html
// https://stackoverflow.com/questions/51750377/how-to-disable-interrupt-watchdog-in-esp32-or-increase-isr-time-limit
// Increase IWTD (Interrupt Watchdog Timer) timeout is necessary.
// Make this higher than the FreeRTOS tick rate (wait? the tick rate is 1000Hz i.e. 1ms, so it's not the FreeRTOS blocking)
static const auto RMT_MEM_BLOCK_NUM = 512;
constexpr size_t DECODE_BUFFER_SIZE      = 2048;


enum class LaneStatus {
  FORWARD  = ::LaneStatus_FORWARD,
  BACKWARD = ::LaneStatus_BACKWARD,
  STOP     = ::LaneStatus_STOP,
  BLINK   = ::LaneStatus_BLINK,
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
  /// hidden head for calculation
  meter _head       = meter(0);
  meter tail        = meter(0);
  LaneStatus status = LaneStatus::STOP;
  static LaneState zero() {
    return LaneState{
        .shift  = meter(0),
        .speed  = 0,
        .head   = meter(0),
        ._head  = meter(0),
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

// note: I assume every time call this function the time interval is 1/fps
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
  friend class ControlCharCallback;
  friend class ConfigCharCallback;

protected:
  using strip_ptr_t = std::unique_ptr<strip::IStrip>;
  strip_ptr_t strip = nullptr;
  bool is_initialized = false;
  Preferences pref;
  static const neoPixelType pixel_type = NEO_RGB + NEO_KHZ800;
  Adafruit_NeoPixel *pixels = nullptr;
  int pin                                          = 23;


  /// in meter
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

  Lane() = default;

  /// iterate the strip
  void iterate();

  void stop() const;

public:
  [[nodiscard]] meter lengthPerLED() const;
  [[nodiscard]] auto getLaneLEDsNum() const {
    return this->cfg.line_LEDs_num;
  }

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

  void setStrip(strip_ptr_t strip) {
    this->strip = std::move(strip);
  };

  /**
   * @brief sets the maximum number of LEDs that can be used. i.e. Circle Length.
   * @warning This function will NOT set the corresponding bluetooth characteristic value.
   * @param new_max_LEDs
   * @param [in,out]cfg mutate in place
   */
  void setMaxLEDs(uint32_t new_max_LEDs);

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

  esp_err_t begin();

  void setConfig(LaneConfig cfg) {
    this->cfg = cfg;
  };

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
  float LEDsPerMeter() const;
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
  /// would expect LaneConfig variant
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override;
  /// would output LaneConfigRO variant
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;

  explicit ConfigCharCallback(lane::Lane &lane) : lane(lane) {}
};
};

#endif // HELLO_WORLD_STRIP_H
