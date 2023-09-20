//
// Created by Kurosu Chan on 2022/8/4.
//

#include "ScanCallback.h"
#include <string_view>
#include <charconv>
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <etl/optional.h>
#include <etl/span.h>
#include <esp_check.h>

static auto TAG        = "AdCallback";
static auto NOTIFY_TAG = "NotifyCallback";

struct ScanCallbackParam {
  std::function<void()> *cb;
  TaskHandle_t handle;
  ~ScanCallbackParam() {
    if (handle != nullptr) {
      vTaskDelete(handle);
    }
    delete cb;
  }
};

// see hr_data.ksy. would mutate the output
etl::optional<size_t> encode(const std::string &updated_id, const DeviceMap &device_map, etl::span<uint8_t> &output) {
  auto sz = device_map.size();
  // offset is the offset of next byte to be written
  size_t offset  = 0;
  output[offset] = static_cast<uint8_t>(sz);
  offset += 1;
  auto first_three_byte = std::string_view(updated_id).substr(0, 3);
  for (auto c : first_three_byte) {
    output[offset] = c;
    offset += 1;
  }
  for (auto &[id, info_p] : device_map) {
    if (info_p != nullptr) {
      auto &info   = *info_p;
      auto f3bytes = std::string_view(id).substr(0, 3);
      for (auto c : f3bytes) {
        output[offset] = c;
        offset += 1;
      }
      output[offset] = info.last_hr;
      if (offset > output.size()) {
        ESP_LOGE(NOTIFY_TAG, "expected size: %d, offset: %d", output.size(), offset);
        return etl::nullopt;
      }
      offset += 1;
    }
  }
  return offset;
}

size_t sizeNeeded(const DeviceMap &device_map) {
  return 4 + device_map.size() * 4;
}

void ScanCallback::onResult(BLEAdvertisedDevice *advertisedDevice) {
  auto name    = advertisedDevice->getName();
  auto payload = std::string(reinterpret_cast<const char *>(advertisedDevice->getPayload()), 31);
  auto pHrChar = this->hr_char;
  if (advertisedDevice->getName().find("T03") != std::string::npos) {
    ESP_LOGI(TAG, "Name: %s, Data: %s, RSSI: %d", name.c_str(),
             to_hex(payload.c_str(), 31).c_str(),
             advertisedDevice->getRSSI());
    auto &device_map = this->devices;
    // return the start of the band id (index)
    // add with additional space
    auto brand_pos    = name.find("T03") + 4;
    auto band_id_str  = name.substr(brand_pos, name.length() - brand_pos);
    bool is_scanned   = device_map.find(band_id_str) != device_map.end();
    bool is_connected = [is_scanned, band_id_str, &device_map]() {
      if (is_scanned) {
        auto &client = *device_map.at(band_id_str)->client;
        return client.isConnected();
      }
      return false;
    }();
    if (is_connected) {
      return;
    }

    // allocate in heap
    auto cb = [advertisedDevice, name, band_id_str, pHrChar, &device_map]() {
      /// user should check the return value of this function
      auto configClient = [advertisedDevice, &name](BLEClient &client) -> NimBLERemoteCharacteristic * {
        const auto serviceUUID   = "180D";
        const auto heartRateUUID = "2A37";
        client.connect(advertisedDevice);
        auto *pService = client.getService(serviceUUID);
        if (pService == nullptr) {
          ESP_LOGE(TAG, "Failed to find service UUID: %s for %s", serviceUUID, name.c_str());
          client.disconnect();
          return nullptr;
        }
        auto *pCharacteristic = pService->getCharacteristic(heartRateUUID);
        if (pCharacteristic == nullptr) {
          ESP_LOGE(TAG, "Failed to find HR characteristic UUID: %s for %s", heartRateUUID, name.c_str());
          client.disconnect();
          return nullptr;
        }

        if (!pCharacteristic->canNotify()) {
          ESP_LOGE(TAG, "HR Characteristic cannot notify for %s", name.c_str());
          client.disconnect();
          return nullptr;
        }
        // pCharacteristic->subscribe(true, notify);
        ESP_LOGI(TAG, "Connected to %s", name.c_str());
        return pCharacteristic;
      };
      /// make a notify callback
      auto make_notify = [band_id_str, pHrChar, &device_map](DeviceInfo *info) {
        return [band_id_str, info, pHrChar, &device_map](
                   NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
                   const uint8_t *pData,
                   size_t length,
                   bool isNotify) {
          if (length >= 2) {
            // the first byte is always 0x04. the second byte is the heart rate.
            auto hr = pData[1];
            if (hr != 0) {
              if (info != nullptr) {
                info->last_hr   = hr;
                info->last_seen = esp_timer_get_time();
                ESP_LOGI(NOTIFY_TAG, "%d bpm from %s", hr, band_id_str.c_str());
                auto buf  = new uint8_t[sizeNeeded(device_map)];
                auto span = etl::span<uint8_t>(buf, sizeNeeded(device_map));
                auto sz   = encode(band_id_str, device_map, span);
                if (sz.has_value()) {
                  if (pHrChar != nullptr) {
                    pHrChar->setValue(span.data(), sz.value());
                    pHrChar->notify();
                  } else {
                    ESP_LOGE(NOTIFY_TAG, "HR characteristic is null");
                  }
                } else {
                  ESP_LOGE(NOTIFY_TAG, "Failed to encode the data");
                }
                // https://cplusplus.com/reference/new/operator%20delete[]/
                // don't use span now
                delete[] buf;
              } else {
                ESP_LOGE(NOTIFY_TAG, "Info is null. Remove the device.");
                device_map.erase(band_id_str);
                pBLERemoteCharacteristic->unsubscribe();
              }
            }
          }
        };
      };
      if (device_map.find(band_id_str) == device_map.end()) {
        ESP_LOGI(TAG, "Configure new band %s", band_id_str.c_str());
        auto &client = *BLEDevice::createClient();
        /// Note, intended to be allocated in heap
        auto info = new DeviceInfo{
            &client,
            esp_timer_get_time(),
            0,
        };
        auto pChar = configClient(client);
        if (pChar != nullptr) {
          auto notify = make_notify(info);
          pChar->subscribe(true, notify);
          device_map.insert(etl::pair(band_id_str, info));
        } else {
          delete info;
        }
      } else {
        ESP_LOGI(TAG, "Reconfigure known band %s", band_id_str.c_str());
        auto info_n = device_map.at(band_id_str);
        if (info_n != nullptr) {
          auto &info   = *info_n;
          auto &client = *info.client;
          auto pChar   = configClient(client);
          auto notify  = make_notify(&info);
          // What would happen to the old notify callback?
          if (pChar != nullptr) {
            pChar->subscribe(true, notify);
          }
        } else {
          ESP_LOGE(TAG, "Info is null. Remove the device.");
          device_map.erase(band_id_str);
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
      // handled by the destructor
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
