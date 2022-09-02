// necessary for using fmt library
#define FMT_HEADER_ONLY

// fmt library should be included first
#include "utils.h"
#include <cstdio>
#include <vector>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "StripCommon.h"
#include "AdCallback.h"

extern "C" { void app_main(); }

static auto esp_name = "e-track 011";

class Strip;

//In seconds
static const int scanTime = 1;
static const int scanInterval = 50;


[[noreturn]]
void scanTask(BLEScan *pBLEScan) {
  for (;;) {
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    printf("Devices found: %d\n", foundDevices.getCount());
    printf("Scan done!\n");
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
    vTaskDelay(scanInterval / portTICK_PERIOD_MS); // Delay a second between loops.
  }

  vTaskDelete(nullptr);
}

void app_main(void) {
  const int DEFAULT_NUM_LEDs = 4000;
  const uint16_t LED_PIN = 14;
  initArduino();

  Preferences pref;
  pref.begin("record", true);
  auto max_LEDs = DEFAULT_NUM_LEDs;
  auto brightness = pref.getUChar("brightness", 32);

  NimBLEDevice::init(esp_name);
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
  pStrip->begin(max_LEDs, LED_PIN, brightness);
  pStrip->initBLE(pServer);

  auto pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(esp_name);
  pAdvertising->setAppearance(0x0340);
  pAdvertising->setScanResponse(false);

  xTaskCreate(reinterpret_cast<TaskFunction_t>(*pFunc),
              "stripTask", 5000,
              pStrip, 2,
              nullptr);

  pServer->start();
  NimBLEDevice::startAdvertising();
  printf("Characteristic defined! Now you can read it in your phone!\n");
  pref.end();
}

