#include <esp_check.h>
#include "led_strip.h"

namespace strip {
// Just a declaration
// I don't use C++ inheritance and too lazy to use any polymorphism library
// maybe some day
class IStrip {
public:
  virtual bool fill_forward(size_t start, size_t count, uint32_t color)                = 0;
  virtual bool fill_backward(size_t total, size_t start, size_t count, uint32_t color) = 0;
  virtual bool clear()                                                                 = 0;
  virtual bool show()                                                                  = 0;
  virtual bool begin()                                                                 = 0;
  virtual bool set_length(size_t length)                                               = 0;
};

class LedStripEsp : public IStrip {
private:
#if SOC_RMT_SUPPORT_DMA
  bool is_dma = true;
#else
  bool is_dma = false;
#endif
  // the resolution is the clock frequency instead of strip frequency
  const uint32_t LED_STRIP_RMT_RES_HZ = (10 * 1000 * 1000); // 10MHz
  led_strip_config_t strip_config     = {
          .strip_gpio_num   = GPIO_NUM_NC,          // The GPIO that connected to the LED strip's data line
          .max_leds         = 0,                    // The number of LEDs in the strip,
          .led_pixel_format = LED_PIXEL_FORMAT_RGB, // Pixel format of your LED strip
          .led_model        = LED_MODEL_WS2812,     // LED strip model
          .flags            = {
                         .invert_out = false // whether to invert the output signal
      },
  };
  led_strip_rmt_config_t rmt_config = {
      .clk_src           = RMT_CLK_SRC_DEFAULT,  // different clock source can lead to different power consumption
      .resolution_hz     = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
      .mem_block_symbols = SOC_RMT_TX_CANDIDATES_PER_GROUP * 64,
      .flags             = {
                      .with_dma = is_dma // DMA feature is available on ESP target like ESP32-S3
      },
  };
  led_strip_handle_t strip_handle = nullptr;

  static esp_err_t led_strip_set_many_pixels(led_strip_handle_t handle, int start, int count, uint32_t color) {
    const auto TAG = "led_strip";
    auto r         = (color >> 16) & 0xff;
    auto g         = (color >> 8) & 0xff;
    auto b         = color & 0xff;
    for (std::integral auto i : std::ranges::iota_view(start, start + count)) {
      // theoretically, I should be unsigned and never be negative.
      // However, I can safely ignore the negative value and continue the iteration.
      if (i < 0) {
        ESP_LOGW(TAG, "Invalid index %d", i);
        continue;
      }
      ESP_RETURN_ON_ERROR(led_strip_set_pixel(handle, i, r, g, b), TAG, "Failed to set pixel %d", i);
    }
    return ESP_OK;
  }

public:
  LedStripEsp() = default;
  LedStripEsp(led_strip_config_t strip_config, led_strip_rmt_config_t rmt_config) : strip_config(strip_config), rmt_config(rmt_config) {}
  ~LedStripEsp() {
    if (strip_handle != nullptr) {
      led_strip_del(strip_handle);
    }
  }
  bool begin() override {
    return led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_handle) == ESP_OK;
  }
  bool fill_forward(size_t start, size_t count, uint32_t color) override {
    if (strip_handle == nullptr) {
      return false;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_clean_clear(strip_handle));
    return led_strip_set_many_pixels(strip_handle, start, count, color) == ESP_OK;
  }

  bool fill_backward(size_t total, size_t start, size_t count, uint32_t color) override {
    if (strip_handle == nullptr) {
      return false;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_clean_clear(strip_handle));
    return led_strip_set_many_pixels(strip_handle, total - start - count, count, color) == ESP_OK;
  }

  bool clear() override {
    if (strip_handle == nullptr) {
      return false;
    }
    return led_strip_clean_clear(strip_handle) == ESP_OK;
  }

  bool show() override {
    if (strip_handle == nullptr) {
      return false;
    }
    return led_strip_refresh(strip_handle) == ESP_OK;
  }

  bool set_length(size_t length) override {
    if (strip_handle != nullptr) {
      led_strip_del(strip_handle);
      strip_handle = nullptr;
    }

    strip_config.max_leds = length;
    return led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_handle) == ESP_OK;
  }
};
// class AdafruitPixel : public IStrip {
//
//
// };
}

