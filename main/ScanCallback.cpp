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
#include <etl/algorithm.h>
#include <esp_check.h>
#include <lwip/def.h>

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

using hr_pair_t = etl::pair<std::string, uint8_t>;

// see hr_data.ksy. would mutate the output
etl::optional<size_t> encode(const hr_pair_t &pair, etl::span<uint8_t> &output) {
  auto &[name, hr] = pair;
  auto id_len      = name.size();
  if (id_len > 255) {
    return etl::nullopt;
  }
  auto offset    = 0;
  output[offset] = id_len;
  offset += 1;
  for (auto &c : name) {
    output[offset] = c;
    offset += 1;
  }
  output[offset] = hr;
  offset += 1;
  return offset;
}

size_t sizeNeeded(const hr_pair_t &pair) {
  auto &[name, hr] = pair;
  return name.size() + 2;
}

struct WatchInfo {
  static constexpr auto TIME_WIDTH = 5;
  uint8_t time[TIME_WIDTH]         = {0};
  uint16_t steps                   = 0;
  uint16_t kcal                    = 0;
  uint8_t hr                       = 0;
  // mm/Hg
  uint8_t SBP = 0;
  // mm/Hg
  uint8_t DBP     = 0;
  uint8_t battery = 0;
  // uint16_t / 10 in Celsius
  float temperature = 0;
  uint8_t SpO2      = 0;
  static WatchInfo from_bytes(const uint8_t *data, size_t size) {
    WatchInfo info = {};
    auto offset    = 0;
    std::copy(data, data + TIME_WIDTH, info.time);
    offset += TIME_WIDTH;
    std::copy(data + offset, data + offset + 2, reinterpret_cast<uint8_t *>(&info.steps));
    info.steps = ntohs(info.steps);
    offset += 2;
    std::copy(data + offset, data + offset + 2, reinterpret_cast<uint8_t *>(&info.kcal));
    info.kcal = ntohs(info.kcal);
    offset += 2;
    info.hr = data[offset];
    offset += 1;
    info.SBP = data[offset];
    offset += 1;
    info.DBP = data[offset];
    offset += 1;
    info.battery = data[offset];
    offset += 1;
    offset += 2; // ignore
    uint16_t temp;
    std::copy(data + offset, data + offset + 2, reinterpret_cast<uint8_t *>(&temp));
    temp             = ntohs(temp);
    info.temperature = static_cast<float>(temp) / 10.0f;
    offset += 2;
    offset += 2; // ignore
    info.SpO2 = data[offset];
    offset += 1;
    // ignore rest
    return info;
  }
};

std::string to_string(const WatchInfo &info) {
  std::stringstream ss;
  // clang-format off
  ss << "steps=" << info.steps <<
      "; kcal=" << info.kcal <<
      "; HR=" << static_cast<int>(info.hr) <<
      "; SBP=" << static_cast<int>(info.SBP) <<
      "; DBP=" << static_cast<int>(info.DBP) <<
      "; Battery=" << static_cast<int>(info.battery) <<
      "; Temperature=" << info.temperature <<
      "; SpO2=" << static_cast<int>(info.SpO2);
  // clang-format on
  return ss.str();
}

void ScanCallback::handleBand(BLEAdvertisedDevice *advertisedDevice) {
  auto name    = advertisedDevice->getName();
  auto payload = std::string(reinterpret_cast<const char *>(advertisedDevice->getPayload()), 31);
  auto pHrChar = this->hr_char;
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
      auto &client = *device_map.at(band_id_str).client;
      return client.isConnected();
    }
    return false;
  }();
  if (is_connected) {
    return;
  }

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
    auto notify = [band_id_str, &device_map, pHrChar](
                      NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
                      const uint8_t *pData,
                      size_t length,
                      bool isNotify) {
      if (length >= 2) {
        // the first byte is always 0x04. the second byte is the heart rate.
        auto hr    = pData[1];
        auto &info = device_map.at(band_id_str);
        if (hr != 0) {
          ESP_LOGI(NOTIFY_TAG, "%d bpm from %s", hr, band_id_str.c_str());
          auto pair = hr_pair_t{band_id_str, hr};
          auto sz   = sizeNeeded(pair);
          auto buf  = new uint8_t[sz];
          auto span = etl::span<uint8_t>(buf, sz);

          auto size = encode(pair, span);
          if (size.has_value()) {
            if (pHrChar != nullptr) {
              pHrChar->setValue(span.data(), size.value());
              pHrChar->notify();
            } else {
              ESP_LOGE(NOTIFY_TAG, "HR characteristic is null");
            }
          } else {
            ESP_LOGE(NOTIFY_TAG, "Failed to encode the data");
          }
          delete[] buf;
        }
      }
    };
    if (device_map.find(band_id_str) == device_map.end()) {
      ESP_LOGI(TAG, "Configure new band %s", band_id_str.c_str());
      auto &client = *BLEDevice::createClient();
      /// Note, intended to be allocated in heap
      auto info = DeviceInfo{
          &client,
      };
      auto pChar = configClient(client);
      if (pChar != nullptr) {
        device_map.insert(etl::pair(band_id_str, std::move(info)));
        auto info_ref = device_map.at(band_id_str);
        /// the ownership of "info" should always be device_map
        pChar->subscribe(true, notify);
      } else {
        ESP_LOGE(TAG, "Failed to configure the band %s", band_id_str.c_str());
      }
    } else {
      ESP_LOGI(TAG, "Reconfigure known band %s", band_id_str.c_str());
      auto info    = device_map.at(band_id_str);
      auto &client = *info.client;
      auto pChar   = configClient(client);
      // What would happen to the old notify callback?
      if (pChar != nullptr) {
        pChar->subscribe(true, notify);
      }
    }
  };

  auto param = new ScanCallbackParam{
      new std::function<void()>(cb),
      nullptr};
  // You should run this in a new thread because the callback is blocking.
  // Never block the scanning thread
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

void ScanCallback::handleWatch(BLEAdvertisedDevice *advertisedDevice) {
  // https://bluetoothle.wiki/advertising
  static constexpr auto TAG = "handleWatch";
  auto payload              = etl::span<uint8_t>(advertisedDevice->getPayload(), advertisedDevice->getPayloadLength());
  auto name                 = advertisedDevice->getName();
  auto pattern              = etl::search(payload.begin(), payload.end(), name.begin(), name.end());
  if (pattern != payload.end()) {
    auto msg = etl::span(pattern + name.size() + 2, payload.end());
    ESP_LOGD(TAG, "(%s) %s", to_hex(msg.data(), msg.size()).c_str(), name.c_str());
    auto info = WatchInfo::from_bytes(msg.data(), msg.size());
    ESP_LOGI(TAG, "%s", to_string(info).c_str());
    auto pair = hr_pair_t{name, info.hr};
    auto sz   = sizeNeeded(pair);
    auto buf  = new uint8_t[sz];
    auto span = etl::span<uint8_t>(buf, sz);
    auto size = encode(pair, span);
    if (size.has_value()) {
      if (hr_char != nullptr) {
        hr_char->setValue(span.data(), size.value());
        hr_char->notify();
      } else {
        ESP_LOGE(TAG, "HR characteristic is null");
      }
    } else {
      ESP_LOGE(TAG, "Failed to encode the data");
    }
    delete[] buf;
  }
}

void ScanCallback::onResult(BLEAdvertisedDevice *advertisedDevice) {
  auto name = advertisedDevice->getName();
  auto addr = advertisedDevice->getAddress();
  if (advertisedDevice->getName().find("T03") != std::string::npos) {
    handleBand(advertisedDevice);
    // starts with "Y"
  } else if (advertisedDevice->getName().find('Y') == 0) {
    handleWatch(advertisedDevice);
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
