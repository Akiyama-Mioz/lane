/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// necessary for using fmt library
#define FMT_HEADER_ONLY

#include <cstdio>
#include <functional>
#include <vector>
#include <memory>
#include "fmt/core.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include <iomanip>
#include <sstream>
#include <ArduinoJson.h>

extern "C" { void app_main(); }

auto esp_name = "cross-esp";

const int scanTime = 1; //In seconds
const int LoopInterval = 50;
// string is array as well
const int capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(31);

std::string to_hex(const std::basic_string<char>& s) {
  std::stringstream ss;
  for (auto c: s) {
    ss << std::hex << std::setfill('0') << std::setw(2) << (int) c;
  }
  return ss.str();
}

class AdCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice *advertisedDevice) override {
    if (advertisedDevice->getName().find("T03") != std::string::npos) {
      std::string output;
      StaticJsonDocument<capacity> doc;
      doc["name"] = advertisedDevice->getName();
      doc["rssi"] = advertisedDevice->getRSSI();
      auto arr = doc.createNestedArray("data");
      for(auto c: advertisedDevice->getManufacturerData()) {
        arr.add(c);
      }
      serializeJson(doc, output);
      printf("%s\n", output.c_str());
//      fmt::print("Name: {}, Data: {}, RSSI: {}\n", advertisedDevice->getName(),
//                 to_hex(advertisedDevice->getManufacturerData()),
//                 advertisedDevice->getRSSI());
    }
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

  NimBLEDevice::init(esp_name);
  // return a pointer
  // the actual resource won't be released by RAII
  auto pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdCallback());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setDuplicateFilter(false);
  pBLEScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
  pBLEScan->setInterval(100);
  // Active scan time
  // less or equal setInterval value
  pBLEScan->setWindow(99);
  xTaskCreate(reinterpret_cast<TaskFunction_t>(scanTask),
              "scanTask", 5000,
              static_cast<void *>(pBLEScan), 1,
              nullptr);
}
