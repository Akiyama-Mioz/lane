//
// Created by Kurosu Chan on 2023/11/6.
//

#ifndef TRACK_SHORT_COMMON_H
#define TRACK_SHORT_COMMON_H
#include <driver/gpio.h>

namespace common {
static auto BLE_NAME = "lane-011";
// static const uint16_t LED_PIN = 2;

static const char *BLE_SERVICE_UUID         = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
static const char *BLE_CHAR_HR_SERVICE_UUID = "180d";
static const char *BLE_CHAR_CONTROL_UUID    = "24207642-0d98-40cd-84bb-910008579114";
static const char *BLE_CHAR_CONFIG_UUID     = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
static const char *BLE_CHAR_HEARTBEAT_UUID  = "048b8928-d0a5-43e2-ada9-b925ec62ba27";
static const char *BLE_CHAR_WHITE_LIST_UUID = "12a481f0-9384-413d-b002-f8660566d3b0";
static const char *BLE_CHAR_DEVICE_UUID     = "a2f05114-fdb6-4549-ae2a-845b4be1ac48";

/**
 * @brief some common *constant* definitions for `lane`
 * @note it's called `lanely` since `lane` the namespace has been taken in global namespace
 */
namespace lanely {
  using centimeter                   = utils::length<float, std::centi>;
  using meter                        = utils::length<float, std::ratio<1>>;
  const auto PREF_RECORD_NAME        = "rec";
  const auto PREF_LINE_LENGTH_NAME   = "ll";
  const auto PREF_ACTIVE_LENGTH_NAME = "al";
  const auto PREF_LINE_LEDs_NUM_NAME = "ln";
  const auto PREF_TOTAL_LENGTH_NAME  = "to"; // float
  const auto PREF_COLOR_NAME         = "co"; // uint32_t

  const auto DEFAULT_ACTIVE_LENGTH         = meter(0.6);  // the line would be active for this length
  const auto DEFAULT_LINE_LENGTH           = meter(50);   // line... it would wrap around
  const auto DEFAULT_TARGET_LENGTH         = meter(1000); // like shift
  const auto DEFAULT_LINE_LEDs_NUM         = static_cast<uint32_t>(DEFAULT_LINE_LENGTH.count() * (100 / 3.3));
  const auto DEFAULT_FPS                   = 10;
  constexpr size_t DECODE_BUFFER_SIZE      = 2048;
  static const auto BLUE_TRANSMIT_INTERVAL = std::chrono::milliseconds(1000);
  static const auto HALT_INTERVAL          = std::chrono::milliseconds(500);
  static const neoPixelType PIXEL_TYPE     = NEO_RGB + NEO_KHZ800;
}

namespace pin {
  static const auto LED      = GPIO_NUM_23;
  static const auto SCK      = GPIO_NUM_16;
  static const auto NSS      = GPIO_NUM_4;
  static const auto MOSI     = GPIO_NUM_17;
  static const auto MISO     = GPIO_NUM_5;
  static const auto LoRa_RST = GPIO_NUM_18;
  static const auto BUSY     = GPIO_NUM_19;
  static const auto DIO1     = GPIO_NUM_21;
  static const auto DIO2     = GPIO_NUM_33;
  static const auto DIO3     = GPIO_NUM_26;
}
};

#endif // TRACK_SHORT_COMMON_H
