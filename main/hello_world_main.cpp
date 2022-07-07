/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// necessary for using fmt library
#define FMT_HEADER_ONLY
#include <cstdio>
#include <functional>
#include <memory>
#include "fmt/core.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_spi_flash.h"
#include "Arduino.h"
#include "NimBLEDevice.h"

extern "C" { void app_main(); }

const int scanTime = 5; //In seconds
const int LoopInterval = 100;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice *advertisedDevice) override {
//    printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());
    fmt::print("Advertised Device: {}\n", advertisedDevice->toString());
  }
};

[[noreturn]] void scanTask(BLEScan *pBLEScan) {
  for (;;) {
    // put your main code here, to run repeatedly:
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    printf("Devices found: %d\n", foundDevices.getCount());
    printf("Scan done!\n");
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
    vTaskDelay(LoopInterval / portTICK_PERIOD_MS); // Delay a second between loops.
  }

  vTaskDelete(nullptr);
}

void app_main() {
  initArduino();
  printf("Hello world!\n");

  NimBLEDevice::init("crosstyan");
  // return a pointer
  // the actual resource won't be released by RAII
  auto pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
  pBLEScan->setDuplicateFilter(false);
  pBLEScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
  pBLEScan->setInterval(100);
  // Active scan time
  // less or equal setInterval value
  // pBLEScan->setWindow(10);
  xTaskCreate(reinterpret_cast<TaskFunction_t>(scanTask),
              "scanTask", 5000,
              static_cast<void *>(pBLEScan), 1,
              nullptr);
}
