// fmt library should be included first
#include "utils.h"
#include <vector>
#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "Lane.h"
#include "ScanCallback.h"

static auto BLE_NAME          = "lane-011";
static const uint16_t LED_PIN = 23;
// static const uint16_t LED_PIN = 2;

static const char *BLE_SERVICE_UUID         = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
static const char *BLE_CHAR_HR_SERVICE_UUID = "180d";
static const char *BLE_CHAR_CONTROL_UUID    = "24207642-0d98-40cd-84bb-910008579114";
static const char *BLE_CHAR_CONFIG_UUID     = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
static const char *BLE_CHAR_HEARTBEAT_UUID  = "048b8928-d0a5-43e2-ada9-b925ec62ba27";

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

extern "C" [[noreturn]] void app_main() {
  initArduino();

  Preferences pref;
  pref.begin(PREF_RECORD_NAME, true);
  auto line_length   = pref.getFloat(PREF_LINE_LENGTH_NAME, DEFAULT_LINE_LENGTH.count());
  auto active_length = pref.getFloat(PREF_ACTIVE_LENGTH_NAME, DEFAULT_ACTIVE_LENGTH.count());
  auto line_LEDs_num = pref.getULong(PREF_LINE_LEDs_NUM_NAME, DEFAULT_LINE_LEDs_NUM);
  auto total_length  = pref.getFloat(PREF_TOTAL_LENGTH_NAME, DEFAULT_TARGET_LENGTH.count());
  auto color         = pref.getULong(PREF_COLOR_NAME, utils::Colors::Red);
  pref.end();
  auto default_cfg = lane::LaneConfig{
      .color         = color,
      .line_length   = lane::meter(line_length),
      .active_length = lane::meter(active_length),
      .total_length  = lane::meter(total_length),
      .line_LEDs_num = line_LEDs_num,
      .fps           = DEFAULT_FPS,
  };

  NimBLEDevice::init(BLE_NAME);
  auto &server = *NimBLEDevice::createServer();
  server.setCallbacks(new ServerCallbacks());

  /**** Initialize NeoPixel. ****/
  /** an aux function used to let FreeRTOS do it work.
   * since FreeRTOS is implemented in C where we can't have lambda capture, so pStrip must be
   * passed as parameter.
   **/
  auto lane_loop = [](void *param) {
    auto &lane = *static_cast<Lane *>(param);
    lane.loop();
  };
  // using singleton pattern to avoid memory leak
  auto &lane    = *Lane::get();
  auto lane_ble = LaneBLE();
  initBLE(&server, lane_ble, lane);
  lane.setBLE(lane_ble);
  lane.setConfig(default_cfg);
  ESP_ERROR_CHECK(lane.begin(LED_PIN));

  //************** HR char initialization ****************

  auto &hr_service = *server.createService(BLE_CHAR_HR_SERVICE_UUID);
  auto &hr_char    = *hr_service.createCharacteristic(BLE_CHAR_HEARTBEAT_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  hr_service.start();

  auto &ad = *NimBLEDevice::getAdvertising();
  ad.setName(BLE_NAME);
  ad.setScanResponse(false);

  // https://github.com/espressif/esp-idf/issues/11651
  // https://lang-ship.com/reference/unofficial/M5StickC/Functions/freertos/task/
  xTaskCreatePinnedToCore(lane_loop,
              "lane", 5120,
              &lane, configMAX_PRIORITIES - 3,
              nullptr, 1);

  server.start();
  NimBLEDevice::startAdvertising();
  auto &scan   = *BLEDevice::getScan();
  auto pScanCb = new ScanCallback(&hr_char);
  scan.setScanCallbacks(pScanCb);
  scan.setInterval(1349);
  scan.setWindow(449);
  scan.setActiveScan(true);
  const auto scanTime = std::chrono::milliseconds(3000);
  // scan time + sleep time
  const auto scanTotalTime = std::chrono::milliseconds(5000);
  ESP_LOGI("MAIN", "Initiated");
  pref.end();
  for (;;) {
    scan.start(scanTime.count(), false);
    vTaskDelay(scanTotalTime.count() / portTICK_PERIOD_MS);
  }
}