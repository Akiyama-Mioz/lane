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
#include "c++/8.4.0/map"
#include "c++/8.4.0/vector"
#include "lane.pb.h"
#include "pb_common.h"
#include "pb_decode.h"
#include "Strip.hpp"
#include <c++/8.4.0/memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Adafruit_NeoPixel.h"
#include "common.h"

namespace lane {

using centimeter = common::lanely::centimeter;
using meter      = common::lanely::meter;

struct notify_timer_param {
  std::function<void()> fn;
};

enum class LaneStatus {
  FORWARD  = ::LaneStatus_FORWARD,
  BACKWARD = ::LaneStatus_BACKWARD,
  STOP     = ::LaneStatus_STOP,
  BLINK    = ::LaneStatus_BLINK,
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
  meter head = meter(0);
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
  /// @brief the length of the line
  meter line_length;
  /// @brief the length of the active part of the line
  meter active_length;
  /// @brief after this length, the line would be stop and reset
  meter finish_length;
  /// @brief the number of LEDs that form the line
  uint32_t line_LEDs_num;
  float fps;
};

/**
 * @brief input; external, outside world could change it
 * (instead of changing the state directly)
 */
struct LaneParams {
  float speed;
  LaneStatus status;
};

// note: I assume every time call this function the time interval is 1/fps
std::tuple<LaneState, LaneParams>
static nextState(const LaneState &last_state, const LaneConfig &cfg, const LaneParams &input);

//**************************************** Lane *********************************/

/**
 * @brief The Lane class
 */
class Lane {
  friend class ControlCharCallback;
  friend class ConfigCharCallback;

private:
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

  /**
   * @brief Bluetooth LE and lane related stuff
   */
  struct LaneBLE {
    Lane *lane                        = nullptr;
    NimBLECharacteristic *ctrl_char   = nullptr;
    NimBLECharacteristic *config_char = nullptr;

    NimBLEService *service = nullptr;
    ControlCharCallback ctrl_cb;
    ConfigCharCallback config_cb;
    explicit LaneBLE(Lane *lane) : lane(lane), ctrl_cb(*lane), config_cb(*lane) {}
  };

  Preferences pref;
  using strip_ptr_t = std::unique_ptr<strip::IStrip>;
  strip_ptr_t strip = nullptr;
  notify_timer_param timer_param{[]() {}};
  TimerHandle_t timer_handle = nullptr;
  std::array<uint8_t, common::lanely::DECODE_BUFFER_SIZE>
      decode_buffer = {0};

  LaneBLE ble    = LaneBLE{this};
  LaneConfig cfg = {
      .color         = utils::Colors::Red,
      .line_length   = common::lanely::DEFAULT_LINE_LENGTH,
      .active_length = utils::length_cast<meter>(common::lanely::DEFAULT_ACTIVE_LENGTH),
      .finish_length = common::lanely::DEFAULT_TARGET_LENGTH,
      .line_LEDs_num = common::lanely::DEFAULT_LINE_LEDs_NUM,
      .fps           = common::lanely::DEFAULT_FPS,
  };
  LaneState state   = LaneState::zero();
  LaneParams params = {
      .speed  = 0,
      .status = LaneStatus::STOP,
  };

  void iterate();

  /**
   * @brief config the characteristic for BLE
   * @param[in] server
   * @param[out] ble the LaneBLE to be written, expected to be initialized
   * @param[in] lane
   * @warning `ctrl_cb` and `config_cb` are static variables, so they are initialized only once.
   * It would be a problem if you have multiple lanes. However, nobody would do that.
   */
  static void _initBLE(NimBLEServer &server, LaneBLE &ble);

  void stop() const;

public:
  explicit Lane(strip_ptr_t strip) : strip(std::move(strip)){};
  [[nodiscard]] meter lengthPerLED() const;
  [[nodiscard]] auto getLaneLEDsNum() const {
    return this->cfg.line_LEDs_num;
  }

  /**
   * @brief Initialize the BLE service.
   * @param server the BLE server
   * @warning
   */
  void initBLE(NimBLEServer &server) {
    if (this->ble.service != nullptr) {
      ESP_LOGE("LANE", "BLE has already been initialized");
      return;
    }
    _initBLE(server, ble);
  }

  /**
   * @brief Loop the strip.
   * @warning This function will never return and you should call this in creatTask/Thread
   *          or something equivalent in RTOS.
   * @param void void
   * @return No return
   */
  [[noreturn]] void loop();

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
   * @param st
   */
  void notifyState(LaneState st);

  esp_err_t begin();

  void setConfig(const LaneConfig &newCfg) {
    this->cfg = newCfg;
  };

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
  [[nodiscard]] float LEDsPerMeter() const;
};

//*********************************** Callbacks ****************************************/

};

#endif // HELLO_WORLD_STRIP_H
