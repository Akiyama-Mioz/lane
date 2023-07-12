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

extern "C" { void app_main(); }

static auto BLE_NAME = "long-track 011";
static const uint16_t LED_PIN = 23;

void app_main() {
  initArduino();

  Preferences pref;
  pref.begin(STRIP_PREF_RECORD_NAME, true);
  auto brightness = pref.getUChar(STRIP_BRIGHTNESS_KEY, 32);
  auto circle_num = pref.getUInt(STRIP_CIRCLE_LEDs_NUM_KEY, STRIP_DEFAULT_CIRCLE_LEDs_NUM);
  auto track_num = pref.getUInt(STRIP_TRACK_LEDs_NUM_KEY, STRIP_DEFAULT_TRACK_LEDs_NUM);
  auto circle_length = pref.getFloat(STRIP_CIRCLE_LENGTH_KEY, STRIP_DEFAULT_CIRCLE_LENGTH);

  NimBLEDevice::init(BLE_NAME);
  auto pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  /**** Initialize NeoPixel. ****/
  /** an aux function used to let FreeRTOS do it work.
   * since FreeRTOS is implemented in C where we can't have lambda capture, so pStrip must be
   * passed as parameter.
  **/
  auto pFunc = [](Strip *pStrip) {
    pStrip->stripTask();
  };
  // using singleton pattern to avoid memory leak
  auto pStrip = Strip::get();
  pStrip->setCircleLength(circle_length);
  pStrip->setCountLEDs(track_num);
  pStrip->setMaxLEDs(circle_num);
  pStrip->begin(LED_PIN, brightness);
  pStrip->initBLE(pServer);

  auto pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(BLE_NAME);
  pAdvertising->setScanResponse(false);

  xTaskCreate(reinterpret_cast<TaskFunction_t>(*pFunc),
              "stripTask", 5120,
              pStrip, configMAX_PRIORITIES - 3,
              nullptr);

  pServer->start();
  NimBLEDevice::startAdvertising();
  ESP_LOGI("MAIN", "Characteristic defined! Now you can read it in your phone!");
  pref.end();
}
