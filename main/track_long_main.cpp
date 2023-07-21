// fmt library should be included first
#include "utils.h"
#include <vector>
#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "StripCommon.h"
#include "AdCallback.h"

extern "C" {
void app_main();
}

static auto BLE_NAME          = "long-track 011";
static const uint16_t LED_PIN = 23;

void app_main() {
  initArduino();

  Preferences pref;
  pref.begin(STRIP_PREF_RECORD_NAME, true);
  auto brightness    = pref.getUChar(STRIP_BRIGHTNESS_KEY, 32);
  auto circle_num    = pref.getUInt(STRIP_CIRCLE_LEDs_NUM_KEY, STRIP_DEFAULT_CIRCLE_LEDs_NUM);
  auto track_num     = pref.getUInt(STRIP_TRACK_LEDs_NUM_KEY, STRIP_DEFAULT_TRACK_LEDs_NUM);
  auto circle_length = pref.getFloat(STRIP_CIRCLE_LENGTH_KEY, STRIP_DEFAULT_FULL_LINE_LENGTH);

  NimBLEDevice::init(BLE_NAME);
  auto &server = *NimBLEDevice::createServer();
  server.setCallbacks(new ServerCallbacks());

  /**** Initialize NeoPixel. ****/
  /** an aux function used to let FreeRTOS do it work.
   * since FreeRTOS is implemented in C where we can't have lambda capture, so pStrip must be
   * passed as parameter.
   **/
  auto pFunc = [](Strip *pStrip) {
    pStrip->stripTask();
  };
  // using singleton pattern to avoid memory leak
  auto &strip = *Strip::get();
  strip.setCircleLength(circle_length);
  strip.setCountLEDs(track_num);
  strip.setMaxLEDs(circle_num);
  strip.begin(LED_PIN, brightness);
  strip.initBLE(&server);

  auto &ad = *NimBLEDevice::getAdvertising();
  ad.setName(BLE_NAME);
  ad.setScanResponse(false);

  xTaskCreate(reinterpret_cast<TaskFunction_t>(*pFunc),
              "stripTask", 5120,
              &strip, configMAX_PRIORITIES - 3,
              nullptr);

  server.start();
  NimBLEDevice::startAdvertising();
  auto &scan = *BLEDevice::getScan();
  // get list of connected devices here
  auto pScanCb = new AdCallback(nullptr);
  scan.setScanCallbacks(pScanCb);
  scan.setInterval(1349);
  scan.setWindow(449);
  scan.setActiveScan(true);
  auto scanTime = std::chrono::milliseconds(3000);
  // scan time + sleep time
  auto scanTotalTime = std::chrono::milliseconds(5000);
  ESP_LOGI("MAIN", "Initiated");
  pref.end();
  // scan forever
  for (;;) {
//    auto founded = scan.getResults(scanTime.count());
//    ESP_LOGI("SCAN", "Found %d devices", founded.getCount());
//    for (auto &device :founded){
//      ESP_LOGI("SCAN", "%s::%s", device->getName().c_str(), device->getAddress().toString().c_str());
//    }
//    scan.clearResults();
    scan.start(scanTime.count(), false);
    vTaskDelay(scanTotalTime.count() / portTICK_PERIOD_MS);
  }
}
