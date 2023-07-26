// fmt library should be included first
#include "utils.h"
#include <vector>
#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "LaneCommon.h"
#include "ScanCallback.h"

static auto BLE_NAME          = "lane-011";
static const uint16_t LED_PIN = 23;

static const char *LIGHT_SERVICE_UUID        = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
static const char *LIGHT_CHAR_STATUS_UUID    = "24207642-0d98-40cd-84bb-910008579114";
static const char *LIGHT_CHAR_CONFIG_UUID    = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
static const char *LIGHT_CHAR_STATE_UUID     = "ed3eefa1-3c80-b43f-6b65-e652374650b5";
static const char *LIGHT_CHAR_HEARTBEAT_UUID = "048b8928-d0a5-43e2-ada9-b925ec62ba27";

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
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
                                                        NIMBLE_PROPERTY::NOTIFY);
  auto ctrl_cb  = new ControlCharCallback(lane);
  ble.ctrl_char->setCallbacks(ctrl_cb);

  /// write to control and read/notify for the state
  ble.config_char = ble.service->createCharacteristic(LIGHT_CHAR_CONFIG_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  auto config_cb  = new ConfigCharCallback(lane);
  ble.config_char->setCallbacks(config_cb);

  ble.service->start();
}

extern "C" void app_main() {
  initArduino();

  Preferences pref;
  pref.begin(LANE_PREF_RECORD_NAME, true);
  auto brightness    = pref.getUChar(LANE_BRIGHTNESS_KEY, 32);
  auto circle_num    = pref.getUInt(LANE_CIRCLE_LEDs_NUM_KEY, LANE_DEFAULT_CIRCLE_LEDs_NUM);
  auto track_num     = pref.getUInt(LANE_TRACK_LEDs_NUM_KEY, LANE_DEFAULT_TRACK_LEDs_NUM);
  auto circle_length = pref.getFloat(LANE_CIRCLE_LENGTH_KEY, LANE_DEFAULT_LINE_LENGTH);

  NimBLEDevice::init(BLE_NAME);
  auto &server = *NimBLEDevice::createServer();
  server.setCallbacks(new ServerCallbacks());

  /**** Initialize NeoPixel. ****/
  /** an aux function used to let FreeRTOS do it work.
   * since FreeRTOS is implemented in C where we can't have lambda capture, so pStrip must be
   * passed as parameter.
   **/
  auto pFunc = [](Lane *pStrip) {
    pStrip->loop();
  };
  // using singleton pattern to avoid memory leak
  auto &lane = *Lane::get();
  lane.setCircleLength(circle_length);
  lane.setCountLEDs(track_num);
  lane.setMaxLEDs(circle_num);
  lane.begin(LED_PIN, brightness);
  auto lane_ble = LaneBLE();
  lane.setBLE(lane_ble);
  initBLE(&server, lane_ble, lane);
  auto &lane_ble_service = *lane_ble.service;
  auto &hr_char          = *lane_ble_service.createCharacteristic(LIGHT_CHAR_STATE_UUID,
                                                                  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  auto &ad = *NimBLEDevice::getAdvertising();
  ad.setName(BLE_NAME);
  ad.setScanResponse(false);

  xTaskCreate(reinterpret_cast<TaskFunction_t>(*pFunc),
              "lane", 5120,
              &lane, configMAX_PRIORITIES - 3,
              nullptr);

  server.start();
  NimBLEDevice::startAdvertising();
  auto &scan = *BLEDevice::getScan();
  // get list of connected devices here
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
  // scan forever
  for (;;) {
    scan.start(scanTime.count(), false);
    vTaskDelay(scanTotalTime.count() / portTICK_PERIOD_MS);
  }
}
