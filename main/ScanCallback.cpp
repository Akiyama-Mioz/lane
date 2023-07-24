//
// Created by Kurosu Chan on 2022/8/4.
//

#include "ScanCallback.h"
#include <string_view>
#include <charconv>
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static auto TAG        = "AdCallback";
static auto NOTIFY_TAG = "NotifyCallback";

struct ScanCallbackParam {
  std::function<void()> *cb;
  TaskHandle_t handle;
};

void ScanCallback::onResult(BLEAdvertisedDevice *advertisedDevice) {
  auto name    = advertisedDevice->getName();
  auto payload = std::string(reinterpret_cast<const char *>(advertisedDevice->getPayload()), 31);
  if (advertisedDevice->getName().find("T03") != std::string::npos) {
    ESP_LOGI(TAG, "Name: %s, Data: %s, RSSI: %d", name.c_str(),
             to_hex(payload.c_str(), 31).c_str(),
             advertisedDevice->getRSSI());
    auto &device_map  = this->devices;
    // return the start of the band id (index)
    // add with additional space
    auto brand_pos = name.find("T03") + 4;
    auto band_id_str = std::string_view(name).substr(brand_pos, name.length() - brand_pos);
    int band_id;
    auto [ptr, ec] = std::from_chars(band_id_str.data(), band_id_str.data() + band_id_str.size(), band_id);
    if (ec != std::errc()) {
      ESP_LOGE(TAG, "Failed to parse band id: %s", band_id_str.data());
      return;
    }
    bool is_scanned   = device_map.find(band_id) != device_map.end();
    bool is_connected = [is_scanned, band_id, &device_map]() {
      if (is_scanned) {
        auto &client = *device_map.at(band_id);
        return client.isConnected();
      }
      return false;
    }();
    if (is_connected) {
      return;
    }

    // allocate in heap
    auto cb = [advertisedDevice, name, band_id, &device_map]() {
      auto configClient = [advertisedDevice, &name](BLEClient &client) {
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
      if (device_map.find(band_id) == device_map.end()) {
        auto &client = *BLEDevice::createClient();
        configClient(client);
        device_map.insert(etl::pair(band_id, &client));
      } else {
        auto &client = *device_map.at(band_id);
        if (!client.isConnected()) {
          configClient(client);
        }
      }
    };
    auto param = new ScanCallbackParam{
        new std::function<void()>(cb),
        nullptr};
    // You should run this in a new thread because the callback is blocking.
    // Never block the scanning thread
    // copilot generated. I don't expect this will work, but somehow it works.
    auto res = xTaskCreate([](void *cb) {
      auto &param = *reinterpret_cast<ScanCallbackParam *>(cb);
      auto f      = param.cb;
      (*f)();
      delete f;
      if (param.handle != nullptr) {
        vTaskDelete(param.handle);
      }
      delete &param;
    },
                           "connect", 4096, param, 1, &param->handle);
    if (res != pdPASS) {
      ESP_LOGE(TAG, "Failed to create task");
      delete param;
      return;
    }
  }
}

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
