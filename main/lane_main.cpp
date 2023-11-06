#include "inc/utils.h"
#include <vector>
#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "inc/Lane.h"
#include "inc/whitelist.h"
#include <memory.h>
#include "common.h"
#include "inc/ScanCallback.h"

using namespace common;
using namespace lane;
/**
 * @brief config the characteristic for BLE
 * @param[in] server
 * @param[out] ble the LaneBLE to be written, expected to be initialized
 * @param[in] lane
 */
void initBLE(NimBLEServer *server, LaneBLE &ble, Lane &lane) {
  if (server == nullptr) {
    return;
  }

  ble.service = server->createService(BLE_SERVICE_UUID);

  ble.ctrl_char = ble.service->createCharacteristic(BLE_CHAR_CONTROL_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  auto ctrl_cb  = new ControlCharCallback(lane);
  ble.ctrl_char->setCallbacks(ctrl_cb);

  /// write to control and read/notify for the state
  ble.config_char = ble.service->createCharacteristic(BLE_CHAR_CONFIG_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  auto config_cb  = new ConfigCharCallback(lane);
  ble.config_char->setCallbacks(config_cb);

  ble.service->start();
}

extern "C" void app_main() {
  const auto TAG = "main";
  initArduino();

  Preferences pref;
  pref.begin(PREF_RECORD_NAME, true);
  auto line_length   = pref.getFloat(PREF_LINE_LENGTH_NAME, DEFAULT_LINE_LENGTH.count());
  auto active_length = pref.getFloat(PREF_ACTIVE_LENGTH_NAME, DEFAULT_ACTIVE_LENGTH.count());
  auto line_LEDs_num = pref.getULong(PREF_LINE_LEDs_NUM_NAME, DEFAULT_LINE_LEDs_NUM);
  auto total_length  = pref.getFloat(PREF_TOTAL_LENGTH_NAME, DEFAULT_TARGET_LENGTH.count());
  auto color         = pref.getULong(PREF_COLOR_NAME, utils::Colors::Red);
  auto default_cfg   = lane::LaneConfig{
        .color         = color,
        .line_length   = lane::meter(line_length),
        .active_length = lane::meter(active_length),
        .total_length  = lane::meter(total_length),
        .line_LEDs_num = line_LEDs_num,
        .fps           = DEFAULT_FPS,
  };
  pref.end();

  NimBLEDevice::init(BLE_NAME);
  auto &server = *NimBLEDevice::createServer();
  server.setCallbacks(new ServerCallbacks());

  auto lane_task = [](void *param) {
    auto &lane = *static_cast<Lane *>(param);
    lane.loop();
  };
  static auto s = strip::AdafruitPixel(default_cfg.line_LEDs_num, LED_PIN, strip::AdafruitPixel::default_pixel_type);
  auto &lane    = *Lane::get();
  lane.setStrip(std::make_unique<decltype(s)>(std::move(s)));
  auto lane_ble = LaneBLE();
  initBLE(&server, lane_ble, lane);
  lane.setBLE(lane_ble);
  lane.setConfig(default_cfg);
  ESP_ERROR_CHECK(lane.begin());

  /************** HR char initialization ****************/
  auto &hr_service = *server.createService(BLE_CHAR_HR_SERVICE_UUID);
  auto &hr_char    = *hr_service.createCharacteristic(BLE_CHAR_HEARTBEAT_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  hr_service.start();

  auto &ad = *NimBLEDevice::getAdvertising();
  ad.setName(BLE_NAME);
  ad.setScanResponse(false);

  // https://github.com/espressif/esp-idf/issues/11651
  // https://lang-ship.com/reference/unofficial/M5StickC/Functions/freertos/task/
  xTaskCreatePinnedToCore(lane_task,
                          "lane", 5120,
                          &lane, configMAX_PRIORITIES - 3,
                          nullptr, 1);

  server.start();
  NimBLEDevice::startAdvertising();
  ESP_LOGI(TAG, "Initiated");
  vTaskDelete(nullptr);
}
