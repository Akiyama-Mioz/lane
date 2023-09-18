//
// Created by Kurosu Chan on 2022/7/13.
//

#include "Lane.h"
#include <ranges>
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

static esp_err_t led_strip_set_many_pixels(led_strip_handle_t handle, int start, int count, uint32_t color) {
  auto r = (color >> 16) & 0xff;
  auto g = (color >> 8) & 0xff;
  auto b = color & 0xff;
  for (std::integral auto i : std::ranges::iota_view(start, start + count)) {
    // theoretically, i should be unsigned and never be negative.
    // However I can safely ignore the negative value and continue the iteration.
    if (i < 0) {
      ESP_LOGW(TAG, "Invalid index %d", i);
      continue;
    }
    ESP_RETURN_ON_ERROR(led_strip_set_pixel(handle, i, r, g, b), TAG, "Failed to set pixel %d", i);
  }
  return ESP_OK;
}

/**
 *
 * @param handle
 * @param start the distance from the start
 * @param count
 * @param color
 * @return ESP_OK if success
 * @note only fill but not refresh
 */
static esp_err_t fill_forward(led_strip_handle_t handle, size_t start, size_t count, uint32_t color) {
  ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_clean_clear(handle));
  return led_strip_set_many_pixels(handle, start, count, color);
}

/// in parallel with fill_backward. You don't need the total parameter though.
static esp_err_t fill_forward(led_strip_handle_t handle, size_t total, size_t start, size_t count, uint32_t color) {
  ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_clean_clear(handle));
  if (start + count > total) {
    ESP_LOGW(TAG, "Invalid start %d, count %d; %d > %d ", start, count, start + count, total);
  }
  return led_strip_set_many_pixels(handle, start, count, color);
}

/**
 *
 * @param handle
 * @param total
 * @param start the distance from the end
 * @param count
 * @param color
 * @return ESP_OK if success
 */
static esp_err_t fill_backward(led_strip_handle_t handle, size_t total, size_t start, size_t count, uint32_t color) {
  ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_clean_clear(handle));
  return led_strip_set_many_pixels(handle, total - start - count, count, color);
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
  led_strip_clear(led_strip);
  const auto delay = pdMS_TO_TICKS(HALT_INTERVAL.count());
  vTaskDelay(delay);
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
    if (params.status == LaneStatus::FORWARD || params.status == LaneStatus::BACKWARD) {
      instant.reset();
      iterate();
      if (debug_instant.elapsed() > DEBUG_INTERVAL) {
        ESP_LOGI(TAG, "head: %.2f, tail: %.2f, shift: %.2f, speed: %.2f, status: %s, color %0lx06x", state.head.count(), state.tail.count(), state.shift.count(), state.speed, statusToStr(state.status).c_str(), cfg.color);
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
      led_strip_clear(led_strip);
      led_strip_refresh(led_strip);
      vTaskDelay(pdMS_TO_TICKS(HALT_INTERVAL.count()));
    } else if (params.status == LaneStatus::BLINK) {
      auto const BLINK_INTERVAL = std::chrono::milliseconds(500);
      auto delay                = pdMS_TO_TICKS(BLINK_INTERVAL.count());
      led_strip_clear(led_strip);
      vTaskDelay(delay);
      led_strip_set_many_pixels(led_strip, 0, cfg.line_LEDs_num, cfg.color);
      led_strip_refresh(led_strip);
      vTaskDelay(delay);
    } else {
      // unreachable
    }
  }
}

/// i.e. Circle LEDs
void Lane::setMaxLEDs(uint32_t new_max_LEDs) {
  cfg.line_LEDs_num = new_max_LEDs;
  if (led_strip != nullptr) {
    led_strip_del(led_strip);
    led_strip = nullptr;
  }
  led_strip_config_t strip_config = {
      .strip_gpio_num   = pin,              // The GPIO that connected to the LED strip's data line
      .max_leds         = new_max_LEDs,     // The number of LEDs in the strip,
      .led_pixel_format = LED_PIXEL_FORMAT, // Pixel format of your LED strip
      .led_model        = LED_MODEL_WS2812, // LED strip model
      .flags            = {
                     .invert_out = false // whether to invert the output signal
      },
  };

  // LED strip backend configuration: RMT
  led_strip_rmt_config_t rmt_config = {
      .clk_src       = RMT_CLK_SRC_DEFAULT,  // different clock source can lead to different power consumption
      .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
      .mem_block_symbols = RMT_MEM_BLOCK_NUM,
      .flags         = {
                  .with_dma = false // DMA feature is available on ESP target like ESP32-S3
      },
  };
  led_strip_handle_t new_handle;
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &new_handle));
  led_strip = new_handle;
}

/**
 * @brief Get the instance/pointer of the strip.
 * @return the instance/pointer of the strip.
 */
Lane *Lane::get() {
  static auto *strip = new Lane();
  return strip;
}

/**
 * @brief initialize the strip. this function should only be called once.
 * @param PIN the pin of strip, default is 14.
 * @param brightness the default brightness of the strip. default is 32.
 * @return StripError::OK if the strip is not inited, otherwise StripError::HAS_INITIALIZED.
 */
esp_err_t Lane::begin(int16_t PIN) {
  if (!is_initialized) {
    pref.begin(PREF_RECORD_NAME, false);
    this->pin = PIN;

    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num   = pin,                     // The GPIO that connected to the LED strip's data line
        .max_leds         = this->cfg.line_LEDs_num, // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT,        // Pixel format of your LED strip
        .led_model        = LED_MODEL_WS2812,        // LED strip model
        .flags            = {
                       .invert_out = false // whether to invert the output signal
        },
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,  // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .mem_block_symbols = RMT_MEM_BLOCK_NUM,
        .flags         = {
                    .with_dma = false // DMA feature is available on ESP target like ESP32-S3
        },
    };
    led_strip_handle_t handle;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &handle));
    led_strip      = handle;
    is_initialized = true;
    return ESP_OK;
  } else {
    return ESP_ERR_INVALID_STATE;
  }
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
      if (this->my_debug_instant.elapsed() > std::chrono::milliseconds(500)) {
        ESP_LOGI("Lane::iterate", "head: %d, count: %d", head_index, count);
        my_debug_instant.reset();
      }
      ESP_ERROR_CHECK_WITHOUT_ABORT(fill_forward(led_strip, cfg.line_LEDs_num, tail_index, count, cfg.color));
      ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_refresh(led_strip));
      break;
    }
    case LaneStatus::BACKWARD: {
      ESP_ERROR_CHECK_WITHOUT_ABORT(fill_backward(led_strip, cfg.line_LEDs_num, tail_index, count, cfg.color));
      ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_refresh(led_strip));
      break;
    }
    default:
      // unreachable
      return;
  }
}
}
