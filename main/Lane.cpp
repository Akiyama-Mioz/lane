//
// Created by Kurosu Chan on 2022/7/13.
//

#include "Lane.h"
#include "Strip.hpp"
#include <esp_check.h>

static const auto TAG = "LANE";

#if SOC_RMT_SUPPORT_DMA
bool is_dma = true;
#else
bool is_dma = false;
#endif

// the resolution is the clock frequency instead of strip frequency
const auto LED_STRIP_RMT_RES_HZ = (10 * 1000 * 1000); // 10MHz

static inline int meterToLEDsCount(float meter, float LEDs_per_meter) {
  return std::abs(static_cast<int>(std::round(meter * LEDs_per_meter))) + 1;
}

static inline float LEDsCountToMeter(uint32_t count, float LEDs_per_meter) {
  if (count <= 1) {
    return 0;
  } else {
    return static_cast<float>(count - 1) / LEDs_per_meter;
  }
}

namespace lane {
std::string statusToStr(LaneStatus status) {
  static const std::map<LaneStatus, std::string> LANE_STATUS_STR = {
      {LaneStatus::FORWARD, "FORWARD"},
      {LaneStatus::BACKWARD, "BACKWARD"},
      {LaneStatus::STOP, "STOP"},
  };
  return LANE_STATUS_STR.at(status);
}

inline LaneStatus revert_state(LaneStatus state) {
  if (state == LaneStatus::STOP) {
    return LaneStatus::STOP;
  }
  if (state == LaneStatus::FORWARD) {
    return LaneStatus::BACKWARD;
  } else {
    return LaneStatus::FORWARD;
  }
}

/**
 * @brief get the next state. should be a pure function.
 * @param last_state
 * @param cfg
 * @param [in, out]input param
 * @return the next state.
 */
LaneState nextState(LaneState last_state, LaneConfig cfg, LaneParams &input) {
  auto TAG        = "lane::nextState";
  auto zero_state = LaneState::zero();
  auto stop_case  = [=]() {
    switch (input.status) {
      case LaneStatus::FORWARD: {
        auto ret   = zero_state;
        ret.speed  = input.speed;
        ret.status = LaneStatus::FORWARD;
        return ret;
      }
      case LaneStatus::BACKWARD: {
        auto ret   = zero_state;
        ret.speed  = input.speed;
        ret.status = LaneStatus::BACKWARD;
        return ret;
      }
      default:
        return zero_state;
    }
    return zero_state;
  };
  switch (last_state.status) {
    case LaneStatus::STOP: {
      return stop_case();
    }
    case LaneStatus::BLINK: {
      return stop_case();
    }
    default: {
      if (input.status == LaneStatus::STOP) {
        return zero_state;
      }
      if (input.status != last_state.status) {
        ESP_LOGW(TAG, "Invalid status changed from %s to %s", statusToStr(last_state.status).c_str(), statusToStr(input.status).c_str());
        input.status = last_state.status;
      }
      auto ret  = last_state;
      ret.speed = input.speed;
      // I assume every time call this function the time interval is 1/fps
      ret.shift      = last_state.shift + meter(ret.speed / cfg.fps);
      auto temp_head = last_state._head + meter(ret.speed / cfg.fps);
      auto err       = meter(ret.speed / cfg.fps);
      if (temp_head >= (cfg.active_length + cfg.line_length - err)) {
        ret.status = revert_state(last_state.status);
        ret.head   = meter(0);
        ret._head  = ret.head;
        ret.tail   = meter(0);
      } else if (temp_head >= cfg.line_length) {
        ret._head = temp_head;
        ret.head  = cfg.line_length;
        auto t    = temp_head - cfg.active_length;
        ret.tail  = t > cfg.line_length ? cfg.line_length : t;
      } else {
        ret.head       = temp_head;
        ret._head      = temp_head;
        auto temp_tail = temp_head - cfg.active_length;
        ret.tail       = temp_tail > meter(0) ? temp_tail : meter(0);
      }
      return ret;
    }
  }
}

void Lane::stop() const {
  if (strip == nullptr){
    ESP_LOGE(TAG, "strip is null");
    return;
  }
  strip->clear();
  strip->show();
}

struct UpdateTaskParam {
  std::function<void()> *fn;
  TaskHandle_t handle;
  ~UpdateTaskParam() {
    delete fn;
    fn = nullptr;
    if (handle != nullptr) {
      vTaskDelete(handle);
    }
  }
};

void Lane::loop() {
  auto instant                  = Instant();
  auto update_instant           = Instant();
  auto debug_instant            = Instant();
  auto constexpr DEBUG_INTERVAL = std::chrono::seconds(1);
  ESP_LOGI(TAG, "start loop");
  for (;;) {
    if (strip == nullptr){
      ESP_LOGE(TAG, "strip is null");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (params.status == LaneStatus::FORWARD || params.status == LaneStatus::BACKWARD) {
      instant.reset();
      iterate();
      if (debug_instant.elapsed() > DEBUG_INTERVAL) {
        ESP_LOGI(TAG, "head: %.2f, tail: %.2f, shift: %.2f, speed: %.2f, status: %s, color %0x06x", state.head.count(), state.tail.count(), state.shift.count(), state.speed, statusToStr(state.status).c_str(), cfg.color);
        debug_instant.reset();
      }
      // I could use the timer from FreeRTOS, but I prefer SystemClock now.
      if (update_instant.elapsed() > BLUE_TRANSMIT_INTERVAL) {
        auto task = [](void *param) {
          auto &update_param = *static_cast<UpdateTaskParam *>(param);
          (*update_param.fn)();
          // handled in the destructor
          delete &update_param;
        };
        auto &l          = *this;
        auto update_task = [&l]() {
          if (l.ble.ctrl_char == nullptr) {
            ESP_LOGE("Lane::updateTask", "BLE not initialized");
            return;
          }
          l.notifyState(l.state);
        };
        auto param = new UpdateTaskParam{
            .fn     = new std::function<void()>(update_task),
            .handle = nullptr,
        };
        auto res = xTaskCreate(task, "update_state", 4096, param, 1, &param->handle);
        if (res != pdPASS) [[unlikely]] {
          ESP_LOGE(TAG, "Failed to create task: %s", esp_err_to_name(res));
          delete param;
        }
        update_instant.reset();
      }
      auto diff  = std::chrono::duration_cast<std::chrono::milliseconds>(instant.elapsed());
      auto delay = std::chrono::milliseconds(static_cast<uint16_t>(1000 / cfg.fps)) - diff;
      if (delay < std::chrono::milliseconds(0)) [[unlikely]] {
        ESP_LOGW(TAG, "delay timeout %lld", delay.count());
      } else {
        auto ticks = pdMS_TO_TICKS(delay.count());
        vTaskDelay(ticks);
      }
    } else if (params.status == LaneStatus::STOP) {
      this->state = LaneState::zero();
      stop();
      vTaskDelay(pdMS_TO_TICKS(HALT_INTERVAL.count()));
    } else if (params.status == LaneStatus::BLINK) {
      auto const BLINK_INTERVAL = std::chrono::milliseconds(500);
      auto delay                = pdMS_TO_TICKS(BLINK_INTERVAL.count());
      stop();
      vTaskDelay(delay);
      strip->fill_and_show_forward(0, cfg.line_LEDs_num, cfg.color);
      vTaskDelay(delay);
    } else {
      // unreachable
    }
  }
}

/// i.e. Circle LEDs
void Lane::setMaxLEDs(uint32_t new_max_LEDs) {
  if (strip == nullptr){
    ESP_LOGE(TAG, "strip is null");
    return;
  }
  strip->set_max_LEDs(new_max_LEDs);
}

/**
 * @brief Get the instance/pointer of the strip.
 * @return the instance/pointer of the strip.
 */
Lane *Lane::get() {
  static auto *lane = new Lane();
  return lane;
}

/**
 * @brief initialize the strip. this function should only be called once.
 * @return StripError::OK if the strip is not inited, otherwise StripError::HAS_INITIALIZED.
 */
esp_err_t Lane::begin() {
  if (strip == nullptr){
    return ESP_ERR_INVALID_STATE;
  }
  strip->begin();
  return ESP_OK;
}

void Lane::notifyState(LaneState s) {
  const auto TAG = "Lane::notifyState";
  if (this->ble.ctrl_char == nullptr) {
    ESP_LOGE(TAG, "BLE not initialized");
    return;
  }
  auto &notify_char = *this->ble.ctrl_char;
  auto buf          = std::array<uint8_t, LaneState_size>();
  ::LaneState st    = LaneState_init_zero;
  st.head           = s.head.count();
  st.tail           = s.tail.count();
  st.shift          = s.shift.count();
  st.speed          = s.speed;
  st.status         = static_cast<::LaneStatus>(s.status);
  auto stream       = pb_ostream_from_buffer(buf.data(), buf.size());
  auto ok           = pb_encode(&stream, LaneState_fields, &st);
  if (!ok) {
    ESP_LOGE(TAG, "Failed to encode the state");
    return;
  }
  auto h             = to_hex(buf.cbegin(), stream.bytes_written);
  auto current_value = to_hex(notify_char.getValue());
  notify_char.setValue(buf.cbegin(), stream.bytes_written);
  notify_char.notify();
}

/// always success and
void Lane::setCountLEDs(uint32_t count) {
  this->cfg.line_LEDs_num = count;
}

void Lane::setCircleLength(float l) {
  this->cfg.line_length = meter(l);
}

meter Lane::lengthPerLED() const {
  auto l = this->cfg.line_length;
  auto n = this->cfg.line_LEDs_num;
  return l / n;
}

float Lane::LEDsPerMeter() const {
  auto l = this->cfg.line_length.count();
  auto n = this->cfg.line_LEDs_num;
  return n / l;
}

// You will need this patch to your ESP-IDF if you meet a segmentation fault in RTM
// https://github.com/crosstyan/esp-idf/commit/f18e63bef76f9400aa97dfd5f4cd80812c4bfa19
// I have no idea why `tx_chan->cur_trans->encoder` would cause a segmentation fault. (null pointer dereference obviously)
// I guess it's because some data race shit. dereference it and save `rmt_tx_channel_t` to stack could solve it.
void Lane::iterate() {
  if (strip == nullptr){
    ESP_LOGE(TAG, "strip is null");
    return;
  }
  auto next_state = nextState(this->state, this->cfg, this->params);
  // meter
  auto head       = this->state.head.count();
  auto tail       = this->state.tail.count();
  auto length     = head - tail >= 0 ? head - tail : 0;
  auto head_index = meterToLEDsCount(head, LEDsPerMeter());
  auto tail_index = meterToLEDsCount(tail, LEDsPerMeter());
  auto count      = meterToLEDsCount(length, LEDsPerMeter());
  if (head_index > this->cfg.line_LEDs_num) {
    head_index = this->cfg.line_LEDs_num;
  }
  this->state = next_state;
  switch (next_state.status) {
    case LaneStatus::FORWARD: {
      strip->fill_and_show_forward(tail_index, count, cfg.color);
      break;
    }
    case LaneStatus::BACKWARD: {
      strip->fill_and_show_backward(tail_index, count, cfg.color);
      break;
    }
    default:
      return;
  }
}
}
