//
// Created by Kurosu Chan on 2022/7/13.
//

#include "Lane.h"
#include <ranges>

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

static esp_err_t led_strip_set_many_pixels(led_strip_handle_t handle, size_t start, size_t count, uint32_t color) {
  auto r = (color >> 16) & 0xff;
  auto g = (color >> 8) & 0xff;
  auto b = color & 0xff;
  for (std::integral auto i : std::ranges::iota_view(start, start + count)) {
    auto err = led_strip_set_pixel(handle, i, r, g, b);
    if (err != ESP_OK) {
      return err;
    }
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
  led_strip_clean_clear(handle);
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
  led_strip_clean_clear(handle);
  return led_strip_set_many_pixels(handle, total - start - count, count, color);
}

namespace lane {
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
inline LaneState
nextState(LaneState last_state, LaneConfig cfg, LaneParams &input) {
  auto TAG        = "lane::nextState";
  auto zero_state = LaneState::zero();
  switch (last_state.status) {
    case LaneStatus::STOP: {
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
        case LaneStatus::STOP: {
          return zero_state;
        }
      }
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
      auto temp_head = last_state.head + meter(ret.speed / cfg.fps);
      if (temp_head > (cfg.active_length + cfg.line_length)) {
        ret.status = revert_state(last_state.status);
        ret.head   = temp_head - (cfg.active_length + cfg.line_length);
        ret.tail   = meter(0);
      } else if (temp_head > cfg.line_length) {
        ret.head = cfg.line_length;
        ret.tail = temp_head - cfg.line_length;
      } else {
        ret.head       = temp_head;
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

void Lane::loop() {
  auto instant = Instant();
  for (;;) {
    if (state.status != LaneStatus::STOP) {
      instant.reset();
      run();
      auto diff  = std::chrono::duration_cast<std::chrono::milliseconds>(instant.elapsed());
      auto delay = pdMS_TO_TICKS((1000 / cfg.fps - diff.count()));
      vTaskDelay(delay);
    } else {
      led_strip_clear(led_strip);
      led_strip_refresh(led_strip);
      vTaskDelay(pdMS_TO_TICKS(HALT_INTERVAL.count()));
    }
  }
}

/// i.e. Circle LEDs
void Lane::setMaxLEDs(uint32_t new_max_LEDs, LaneConfig &cfg) {
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
LaneError Lane::begin(int16_t PIN, uint8_t brightness) {
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
        .flags         = {
                    .with_dma = false // DMA feature is available on ESP target like ESP32-S3
        },
    };
    led_strip_handle_t handle;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &handle));
    led_strip      = handle;
    is_initialized = true;
    return LaneError::OK;
  } else {
    return LaneError::HAS_INITIALIZED;
  }
}

void Lane::notifyState(LaneState s) {
//  this->status = s;
//  if (status_char != nullptr) {
//    constexpr size_t buf_size = 64;
//    std::array<uint8_t, buf_size> buf{};
//    TrackStatusMsg msg = TrackStatusMsg_init_zero;
//    msg.status         = s;
//    auto ostream       = pb_ostream_from_buffer(buf.data(), buf_size);
//    auto res           = pb_encode(&ostream, TrackStatusMsg_fields, &msg);
//    if (!res) {
//      ESP_LOGE("Strip::setStatusNotify", "pb_encode");
//      return;
//    }
//    status_char->setValue(buf);
//    status_char->notify();
//  }
}

/// always success and
void Lane::setCountLEDs(uint32_t count) {
  this->cfg.line_LEDs_num = count;
}

void Lane::setCircleLength(float l) {
  this->cfg.line_length = meter(l);
}

float Lane::getLEDsPerMeter() const {
  auto l = this->cfg.line_length.count();
  auto n = this->cfg.line_LEDs_num;
  return static_cast<float>(l / n);
}

void Lane::run() {
  auto next_state = nextState(this->state, this->cfg, this->params);
  // meter
  auto head       = this->state.head.count();
  auto tail       = this->state.tail.count();
  auto length     = head - tail >= 0 ? head - tail : 0;
  auto head_index = meterToLEDsCount(head, getLEDsPerMeter());
  auto count      = meterToLEDsCount(length, getLEDsPerMeter());
  this->state     = next_state;
  switch (next_state.status) {
    case LaneStatus::FORWARD: {
      auto err = fill_forward(led_strip, head_index, count, cfg.color);
      if (err != ESP_OK) {
        ESP_LOGE("LANE", "Error when filling forward: %s", esp_err_to_name(err));
      }
      led_strip_refresh(led_strip);
      break;
    }
    case LaneStatus::BACKWARD: {
      auto err = fill_backward(led_strip, cfg.line_LEDs_num, head, count, cfg.color);
      if (err != ESP_OK) {
        ESP_LOGE("LANE", "Error when filling backward: %s", esp_err_to_name(err));
      }
      led_strip_refresh(led_strip);
      break;
    }
    default:
      // unreachable
      return;
  }
}
}
