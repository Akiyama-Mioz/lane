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
#include <map>

#include "Strip.h"
#include "AdCallback.h"

extern "C" { void app_main(); }

static auto esp_name = "e-track 011";

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

const char * SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const char * CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

void app_main(void) {
  const int DEFAULT_NUM_LEDS = 45;
  const int LED_PIN = 14;
  initArduino();

  Preferences pref;
  pref.begin("record", true);
  auto max_leds = pref.getInt("max_leds", DEFAULT_NUM_LEDS);
  auto color = pref.getUInt("color", Adafruit_NeoPixel::Color(255, 0, 255));
  auto brightness = pref.getUChar("brightness", 32);
  printf("max_leds stored in pref: %d\n", max_leds);
  printf("color stored in pref: %x\n", color);
  printf("brightness stored in pref: %d\n", brightness);

  NimBLEDevice::init(esp_name);
  auto pServer = NimBLEDevice::createServer();
  auto pService = pServer->createService(SERVICE_UUID);
  auto pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::NOTIFY
  );
  pServer->setCallbacks(new ServerCallbacks());
  pCharacteristic->setValue(std::string{0x00});

  // Initialize NeoPixel.
  /** an aux function used to let FreeRTOS do it work.
   * since FreeRTOS is implemented in C where we can't have lambda capture, so pStrip must be
   * passed as parameter.
  **/
  auto pFunc = [](Strip *pStrip){
    pStrip->stripTask();
  };
  // using singleton pattern to avoid memory leak
  auto pStrip = Strip::get();
  pStrip->begin(max_leds, LED_PIN, color, brightness);
  pStrip->initBLE(pServer);
  // Ad service must start after Strip service? why?
  pService->start();

  auto pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(esp_name);
  pAdvertising->setAppearance(0x0340);
  pAdvertising->setScanResponse(false);

  // Initialize BLE Scanner.
  auto pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdCallback(pCharacteristic));
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setDuplicateFilter(false); // maybe I should enable it
  pBLEScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
  pBLEScan->setInterval(100);
  // Active scan time
  // less or equal setInterval value
  pBLEScan->setWindow(99);

  xTaskCreate(reinterpret_cast<TaskFunction_t>(scanTask),
              "scanTask", 5000,
              static_cast<void *>(pBLEScan), 1,
              nullptr);
  xTaskCreate(reinterpret_cast<TaskFunction_t>(*pFunc),
              "stripTask", 5000,
              pStrip, 2,
              nullptr);

  NimBLEDevice::startAdvertising();
  printf("Characteristic defined! Now you can read it in your phone!\n");
  pref.end();
}

