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
//static const uint16_t LED_PIN = 2;

static const char *LIGHT_SERVICE_UUID        = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
static const char *LIGHT_CHAR_STATUS_UUID    = "24207642-0d98-40cd-84bb-910008579114";
static const char *LIGHT_CHAR_CONFIG_UUID    = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
static const char *LIGHT_CHAR_STATE_UUID     = "ed3eefa1-3c80-b43f-6b65-e652374650b5";
static const char *LIGHT_CHAR_HEARTBEAT_UUID = "048b8928-d0a5-43e2-ada9-b925ec62ba27";

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

  ble.service = server->createService(LIGHT_SERVICE_UUID);

  ble.ctrl_char = ble.service->createCharacteristic(LIGHT_CHAR_STATUS_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  auto ctrl_cb  = new ControlCharCallback(lane);
  ble.ctrl_char->setCallbacks(ctrl_cb);

  /// write to control and read/notify for the state
  ble.config_char = ble.service->createCharacteristic(LIGHT_CHAR_CONFIG_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  auto config_cb  = new ConfigCharCallback(lane);
  ble.config_char->setCallbacks(config_cb);

  ble.service->start();
}

extern "C" [[noreturn]] void app_main() {
  initArduino();

  Preferences pref;
  pref.begin(PREF_RECORD_NAME, true);
  // TODO: persist the config
  pref.end();

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
  lane.setBLE(lane_ble);
  initBLE(&server, lane_ble, lane);
  ESP_ERROR_CHECK_WITHOUT_ABORT(lane.begin(LED_PIN));

  //************** HR char initialization ****************

  auto &hr_service = *server.createService("180D");
  auto &hr_char    = *hr_service.createCharacteristic(LIGHT_CHAR_STATE_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  hr_service.start();

  auto &ad = *NimBLEDevice::getAdvertising();
  ad.setName(BLE_NAME);
  ad.setScanResponse(false);

  xTaskCreate(lane_loop,
              "lane", 5120,
              &lane, configMAX_PRIORITIES - 3,
              nullptr);

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
