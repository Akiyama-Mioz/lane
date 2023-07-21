//
// Created by Kurosu Chan on 2022/8/4.
//

#include "AdCallback.h"
#include <thread>
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static auto TAG        = "AdCallback";
static auto NOTIFY_TAG = "NotifyCallback";

void AdCallback::onResult(BLEAdvertisedDevice *advertisedDevice) {
  auto name                = advertisedDevice->getName();
  auto payload             = std::string(reinterpret_cast<const char *>(advertisedDevice->getPayload()), 31);
  if (advertisedDevice->getName().find("T03") != std::string::npos) {
    ESP_LOGI(TAG, "Name: %s, Data: %s, RSSI: %d", name.c_str(),
             to_hex(payload.c_str(), 31).c_str(),
             advertisedDevice->getRSSI());
    auto& device_map = this->devices;
    bool is_scanned = device_map.find(name) != device_map.end();
    bool is_connected = [is_scanned, name, &device_map](){
      if(is_scanned){
        auto& client = *device_map.at(name);
        return client.isConnected();
      }
      return false;
    }();
    if (is_connected) {
      return;
    }
    auto cb = [advertisedDevice, name, &device_map](){
      auto configClient = [advertisedDevice, &name](BLEClient& client){
        const auto serviceUUID   = "180D";
        const auto heartRateUUID = "2A37";
        client.connect(advertisedDevice);
        auto *pService = client.getService(serviceUUID);
        if (pService == nullptr) {
          ESP_LOGE(TAG, "Failed to find service UUID: %s for %s", serviceUUID, name.c_str());
          client.disconnect();
          return;
        }
        auto *pCharacteristic = pService->getCharacteristic(heartRateUUID);
        if (pCharacteristic == nullptr) {
          ESP_LOGE(TAG, "Failed to find HR characteristic UUID: %s for %s", heartRateUUID, name.c_str());
          client.disconnect();
          return;
        }

        if (!pCharacteristic->canNotify()) {
          ESP_LOGE(TAG, "HR Characteristic cannot notify for %s", name.c_str());
          client.disconnect();
          return;
        }
        auto notify = [name](
            NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
            const uint8_t *pData,
            size_t length,
            bool isNotify) {
          if (length >= 2) {
            // the first byte is always 0x04. the second byte is the heart rate.
            auto hr = pData[1];
            if (hr != 0) {
              ESP_LOGI(NOTIFY_TAG, "HR: %d from %s", hr, name.c_str());
            }
          }
        };
        pCharacteristic->subscribe(true, notify);
        ESP_LOGI(TAG, "Connected to %s", name.c_str());
      };
      if (device_map.find(name) == device_map.end()) {
        auto &client = *BLEDevice::createClient();
        configClient(client);
        device_map.insert(etl::pair(name, &client));
      } else {
        auto &client = *device_map.at(name);
        if (!client.isConnected()) {
          configClient(client);
        }
      }
    };
    // You should run this in a new thread because the callback is blocking.
    // Never block the scanning thread
    // copilot generated. I don't expect this will work
    xTaskCreate([](void *cb) {
      auto *f = reinterpret_cast<std::function<void()> *>(cb);
      (*f)();
      delete f;
      vTaskDelete(nullptr);
    }, "connect", 4096, new std::function<void()>(cb), 1, nullptr);
  }
}

// void ServerCallbacks::onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
//  ESP_LOGI("onConnect", "Client address: %s", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
//  /** We can use the connection handle here to ask for different connection parameters.
//   *  Args: connection handle, min connection interval, max connection interval
//   *  latency, supervision timeout.
//   *  Units; Min/Max Intervals: 1.25 millisecond increments.
//   *  Latency: number of intervals allowed to skip.
//   *  Timeout: 10 millisecond increments, try for 5x interval time for best results.
//   */
//  pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
//}

/** Alternative onConnect() method to extract details of the connection.
 *  See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
 */
void ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) {
  ESP_LOGI("onConnect", "Client connected.");
  ESP_LOGI("onConnect", "Multi-connect support: start advertising");
  pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 60);
  NimBLEDevice::startAdvertising();
}

void ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) {
  ESP_LOGI("onDisconnect", "Client disconnected - start advertising");
  NimBLEDevice::startAdvertising();
}

void ServerCallbacks::onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) {
  ESP_LOGI("onMTUChange", "MTU updated: %u for connection ID: %s", MTU, connInfo.getIdAddress().toString().c_str());
}
