//
// Created by Kurosu Chan on 2022/7/13.
//

#include "LaneCommon.h"
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
static esp_err_t fill_forward(led_strip_handle_t handle, size_t start, size_t count, uint32_t color){
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
static esp_err_t fill_backward(led_strip_handle_t handle, size_t total, size_t start, size_t count, uint32_t color){
  led_strip_clean_clear(handle);
  return led_strip_set_many_pixels(handle, total - start - count, count, color);
}

namespace lane {
/**
 * @brief get the next state. should be a pure function.
 * @param state the current state.
 * @param retriever
 * @param fps frames per second
 * @return the next state.
 */
inline RunState
nextState(RunState state, const ValueRetriever<float> &retriever, float circleLength, float trackLength, float fps) {
  float position; // late
  float shift = state.shift;
  float speed = retriever.retrieve(static_cast<int>(state.shift));
  float extra = 0; // late
  // distance unit is `meter`
  // total length will not be considered here.
  shift    = shift + speed / fps; // speed per frame in m/s
  position = shift;
  if (position > circleLength) {
    position = fmod(position, circleLength);
    if (position <= trackLength) {
      extra = trackLength - position;
    }
  }
  return RunState{
      position,
      speed,
      shift,
      extra};
}

/**
 * @brief update the strip with the new state and write the pixel data to the strip.
 * @param handle
 * @param fps
 */
inline RunState updateStrip(led_strip_handle_t handle, float circle_length, float track_length, float fps,
                            float LEDs_per_meter) {
  // TODO
}

void Lane::ready(Instant &last_blink) const {
  uint32_t c    = 0;
  auto duration = last_blink.elapsed();
  auto millis   = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  if (millis > READY_INTERVAL) {
    if (c % 2 == 0) {
      led_strip_clear(led_strip);
    } else {
      auto constexpr front_count = 20;
      auto constexpr back_count  = 20;
      auto front_color           = Adafruit_NeoPixel::Color(255, 0, 0);
      auto back_color            = Adafruit_NeoPixel::Color(0, 0, 255);
      led_strip_clean_clear(led_strip);
      led_strip_set_many_pixels(led_strip, 0, front_count, front_color);
      led_strip_set_many_pixels(led_strip, getLaneLEDsNum() - back_count, back_count, back_color);
      led_strip_refresh(led_strip);
    }
    c += 1;
    last_blink.reset();
  }
  constexpr uint delay = pdMS_TO_TICKS(HALT_INTERVAL);
  vTaskDelay(delay);
}

void Lane::stop() const {
  led_strip_clear(led_strip);
  constexpr uint delay = pdMS_TO_TICKS(HALT_INTERVAL);
  vTaskDelay(delay);
}

void Lane::run(std::vector<Track> &tracks) {
}

void Lane::loop() {
  led_strip_clear(led_strip);
  // pixels->show();
  auto instant = Instant();
  for (;;) {
    if (status == TrackStatus_RUN) {
      //
    } else if (status == TrackStatus_STOP) {
      stop();
    } else if (status == TrackStatus_READY) {
      ready(instant);
    }
  }
}

/// i.e. Circle LEDs
void Lane::setMaxLEDs(uint32_t new_max_LEDs) {
  max_LEDs = new_max_LEDs;
  if (led_strip != nullptr) {
    led_strip_del(led_strip);
    led_strip = nullptr;
  }
  led_strip_config_t strip_config = {
      .strip_gpio_num   = pin,              // The GPIO that connected to the LED strip's data line
      .max_leds         = max_LEDs,         // The number of LEDs in the strip,
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
    pref.begin(LANE_PREF_RECORD_NAME, false);
    this->pin = PIN;

    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num   = pin,              // The GPIO that connected to the LED strip's data line
        .max_leds         = max_LEDs,         // The number of LEDs in the strip,
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
    led_strip_handle_t handle;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &handle));
    led_strip      = handle;
    is_initialized = true;
    return LaneError::OK;
  } else {
    return LaneError::HAS_INITIALIZED;
  }
}

void Lane::setStatusNotify(LaneStatus s) {
  // TODO
}

void Lane::setCountLEDs(uint32_t count) {
  this->count_LEDs = count;
}

void Lane::setCircleLength(float meter) {
  circle_length = meter;
}

float Lane::getLEDsPerMeter() const {
  return static_cast<float>(max_LEDs) / static_cast<float>(this->circle_length);
}
}

