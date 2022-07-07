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
#include <ArduinoJson.h>

extern "C" { void app_main(); }

WiFiClient espClient;
PubSubClient client(espClient);
auto esp_name = "cross-esp";

void MQTT_reconnect(PubSubClient pubSubClient) {
  while (!pubSubClient.connected()) {
    printf("Attempting MQTT connection...\n");
    if (pubSubClient.connect(esp_name)) {
      printf("connected\n");
    } else {
      printf("failed, rc=");
      printf("%d",pubSubClient.state());
      printf(" try again in 5 seconds\n");
      // Wait 5 seconds before retrying
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }
}

const char *ssid = "ilikong24g";
const char *password = "1234567890";

const char *mqtt_host = "23.249.16.149";
const int mqtt_port = 1883;

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
      if (WiFi.status() == WL_CONNECTED){
        client.publish(advertisedDevice->getName().c_str(), output.c_str());
      }
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

[[noreturn]] void WiFiStatusCheck() {
  const int wifi_check_interval = 1000;
  for (;;){
    if (likely(WiFi.status() == WL_CONNECTED)) {
      if (unlikely(!client.connected())) {
        MQTT_reconnect(client);
      }
      client.loop();
    } else {
      WiFi.begin(ssid, password);
    }
    vTaskDelay(wifi_check_interval / portTICK_PERIOD_MS);
  }
}

void app_main() {
  initArduino();
  printf("Hello world!\n");

  WiFi.begin(ssid, password);
  client.setServer(mqtt_host, mqtt_port);
  client.connect(esp_name);

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
  xTaskCreate(reinterpret_cast<TaskFunction_t>(WiFiStatusCheck),
              "WiFiStatusCheck", 5 * 1024,
              nullptr, 1,
              nullptr);
}
