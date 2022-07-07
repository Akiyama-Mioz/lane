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
#include "esp_chip_info.h"
#include "esp_spi_flash.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "WiFi.h"
#include <PubSubClient.h>
#include <iomanip>
#include <sstream>

extern "C" { void app_main(); }

const char *ssid = "ilikong24g";
const char *password = "1234567890";

const char *mqtt_server = "23.249.16.149:1883";

const int scanTime = 1; //In seconds
const int LoopInterval = 50;

std::string to_hex(const std::basic_string<char>& s) {
  std::stringstream ss;
  for (auto c: s) {
    ss << std::hex << std::setfill('0') << std::setw(2) << (int) c;
  }
  return ss.str();
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice *advertisedDevice) override {
//    printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());
    if (advertisedDevice->getName().find("T03") != std::string::npos) {
      fmt::print("Name: {}, Data: {}, RSSI: {}\n", advertisedDevice->getName(),
                 to_hex(advertisedDevice->getManufacturerData()),
                 advertisedDevice->getRSSI());
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
  printf("Hello world!\n");

  NimBLEDevice::init("crosstyan");
  // return a pointer
  // the actual resource won't be released by RAII
  auto pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
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
