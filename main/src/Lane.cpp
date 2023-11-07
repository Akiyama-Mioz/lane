//
// Created by Kurosu Chan on 2022/7/13.
//

#include "Lane.h"
#include "Strip.hpp"
#include <esp_check.h>

static const auto TAG = "LANE";

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
  if (strip == nullptr) {
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
  ESP_LOGI(TAG, "loop");
  for (;;) {
    if (strip == nullptr) {
      ESP_LOGE(TAG, "null strip");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    // TODO: use xTimerCreateStatic to avoid heap allocation repeatedly
    auto delete_timer = [this]() {
      if (this->timer_handle != nullptr) {
        xTimerStop(this->timer_handle, portMAX_DELAY);
        xTimerDelete(this->timer_handle, portMAX_DELAY);
        this->timer_handle = nullptr;
      }
    };
    /**
     * @brief try to create a timer if there is no timer running.
     */
    auto try_create_timer = [this]() {
      // https://www.nextptr.com/tutorial/ta1430524603/capture-this-in-lambda-expression-timeline-of-change
      auto notify_fn = [this]() {
        ESP_LOGI(TAG, "head=%.2f; tail=%.2f; shift=%.2f; speed=%.2f; status=%s; color=%0x06x;",
                 state.head.count(), state.tail.count(), state.shift.count(), state.speed,
                 statusToStr(state.status).c_str(), cfg.color);
        this->notifyState(this->state);
      };

      auto run_notify_fn = [](TimerHandle_t handle) {
        auto &param = *static_cast<notify_timer_param *>(pvTimerGetTimerID(handle));
        param.fn();
      };

      // otherwise a timer is running
      if (this->timer_handle == nullptr) {
        this->timer_param.fn = notify_fn;
        this->timer_handle   = xTimerCreate("notify_timer",
                                            pdMS_TO_TICKS(BLUE_TRANSMIT_INTERVAL.count()),
                                            pdTRUE,
                                            &this->timer_param,
                                            run_notify_fn);
        [[unlikely]] if (this->timer_handle == nullptr) {
          ESP_LOGE(TAG, "Failed to create timer");
          abort();
        }
        xTimerStart(this->timer_handle, portMAX_DELAY);
      }
    };

    switch (params.status) {
      case LaneStatus::FORWARD:
      case LaneStatus::BACKWARD: {
        auto interval_ms = std::chrono::milliseconds(static_cast<int64_t>((1 / cfg.fps) * 1000));
        auto instant     = Instant();
        iterate();
        try_create_timer();

        auto e = instant.elapsed();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(e) > interval_ms) {
          ESP_LOGW(TAG, "show timeout %lld ms > %lld ms (%f FPS)", e.count(), interval_ms.count(), cfg.fps);
        } else {
          auto delay = interval_ms - e;
          vTaskDelay(pdMS_TO_TICKS(delay.count()));
        }
      }
      case LaneStatus::STOP: {
        this->state = LaneState::zero();
        delete_timer();
        vTaskDelay(pdMS_TO_TICKS(HALT_INTERVAL.count()));
      }
      case LaneStatus::BLINK: {
        auto const BLINK_INTERVAL = std::chrono::milliseconds(500);
        auto delay                = pdMS_TO_TICKS(BLINK_INTERVAL.count());
        delete_timer();
        stop();
        vTaskDelay(delay);
        strip->fill_and_show_forward(0, cfg.line_LEDs_num, cfg.color);
        vTaskDelay(delay);
      }
      default:
        break;
    }
  }
}

/// i.e. Circle LEDs
void Lane::setMaxLEDs(uint32_t new_max_LEDs) {
  if (strip == nullptr) {
    ESP_LOGE(TAG, "strip is null");
    return;
  }
  strip->set_max_LEDs(new_max_LEDs);
}

/**
 * @brief Get the instance/pointer of the strip.
 * @return the instance/pointer of the strip.
 */
Lane &Lane::get() {
  static auto lane = Lane{};
  return lane;
}

/**
 * @brief initialize the strip. this function should only be called once.
 * @return StripError::OK if the strip is not inited, otherwise StripError::HAS_INITIALIZED.
 */
esp_err_t Lane::begin() {
  if (strip == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  strip->begin();
  return ESP_OK;
}

void Lane::notifyState(LaneState st) {
  const auto TAG = "Lane::notifyState";
  if (this->ble.ctrl_char == nullptr) {
    ESP_LOGE(TAG, "BLE not initialized");
    return;
  }
  auto &notify_char = *this->ble.ctrl_char;
  auto buf          = std::array<uint8_t, LaneState_size>();
  ::LaneState pb_st = LaneState_init_zero;
  pb_st.head        = st.head.count();
  pb_st.tail        = st.tail.count();
  pb_st.shift       = st.shift.count();
  pb_st.speed       = st.speed;
  pb_st.status      = static_cast<::LaneStatus>(st.status);
  auto stream       = pb_ostream_from_buffer(buf.data(), buf.size());
  auto ok           = pb_encode(&stream, LaneState_fields, &pb_st);
  if (!ok) {
    ESP_LOGE(TAG, "Failed to encode the state");
    return;
  }
  auto h = utils::toHex(buf.cbegin(), stream.bytes_written);
  notify_char.setValue(buf.cbegin(), stream.bytes_written);
  notify_char.notify();
}

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

/**
 * @brief iterate the strip to the next state and set the corresponding LEDs.
 * @note this function will block the current task for a short time (for the strip to show the LEDs).
 *       To make the strip to run with certain FPS, please add a delay outside this function
 *       (minus the time used in this function).
 */
void Lane::iterate() {
  if (strip == nullptr) {
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
