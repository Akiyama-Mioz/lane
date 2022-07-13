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
#include <map>
#include <Adafruit_NeoPixel.h>

#include <pb_encode.h>
#include "ble.pb.h"

extern "C" { void app_main(); }

auto esp_name = "e-track 011";

const int scanTime = 1; //In seconds
const int LoopInterval = 50;

std::string to_hex(const std::basic_string<char> &s) {
  std::string res;
  for (auto c: s) {
    res += fmt::format("{:02x}", c);
  }
  return res;
}

std::string to_hex(const char *s, size_t len) {
  std::string res;
  for (size_t i = 0; i < len; i++) {
    res += fmt::format("{:02x}", s[i]);
  }
  return res;
}

class AdCallback : public BLEAdvertisedDeviceCallbacks {
  NimBLECharacteristic *characteristic = nullptr;

  void onResult(BLEAdvertisedDevice *advertisedDevice) override {
    if (advertisedDevice->getName().find("T03") != std::string::npos) {
      fmt::print("Name: {}, Data: {}, RSSI: {}\n", advertisedDevice->getName(),
                 to_hex(reinterpret_cast<const char *>(advertisedDevice->getPayload()), 31),
                 advertisedDevice->getRSSI());

      uint8_t buffer[128];
      BLEAdvertisingData data = BLEAdvertisingData_init_zero;
      // See https://stackoverflow.com/questions/57569586/how-to-encode-a-string-when-it-is-a-pb-callback-t-type
      // zero ended string
      auto encode_string = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
        const char *str = reinterpret_cast<const char *>(*arg);
        if (!pb_encode_tag_for_field(stream, field)) {
          return false;
        } else {
          return pb_encode_string(stream, (uint8_t *) str, strlen(str));
        }
      };

      // fixed size array of 31 elements
      auto encode_ble_ad = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
        const char *str = reinterpret_cast<const char *>(*arg);
        if (!pb_encode_tag_for_field(stream, field)) {
          return false;
        } else {
          return pb_encode_string(stream, (uint8_t *) str, 31);
        }
      };
      pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
      data.name.arg = const_cast<void *>(reinterpret_cast<const void *>(advertisedDevice->getName().c_str()));
      data.name.funcs.encode = encode_string;
      data.manufactureData.arg = const_cast<void *>(reinterpret_cast<const void *>(advertisedDevice->getPayload()));
      data.manufactureData.funcs.encode = encode_ble_ad;
      data.rssi = advertisedDevice->getRSSI();
      bool status = pb_encode(&stream, BLEAdvertisingData_fields, &data);
      auto length = stream.bytes_written;
      if (this->characteristic != nullptr && status) {
        this->characteristic->setValue(buffer, length);
        this->characteristic->notify();
        std::string buf = std::string(reinterpret_cast<char *>(buffer), length);
        fmt::print("Protobuf: {}, Length: {}\n", to_hex(buf), length);
      } else {
        fmt::print("Error: {}\n", status);
      }
    }
  }

public:
  explicit AdCallback(NimBLECharacteristic *c) : characteristic(c) {}
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

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer) override {
    fmt::print("Client connected. ");
    fmt::print("Multi-connect support: start advertising\n");
    NimBLEDevice::startAdvertising();
  };

  /** Alternative onConnect() method to extract details of the connection.
   *  See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
   */
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) override {
    printf("Client address: ");
    fmt::print(NimBLEAddress(desc->peer_ota_addr).toString());
    /** We can use the connection handle here to ask for different connection parameters.
     *  Args: connection handle, min connection interval, max connection interval
     *  latency, supervision timeout.
     *  Units; Min/Max Intervals: 1.25 millisecond increments.
     *  Latency: number of intervals allowed to skip.
     *  Timeout: 10 millisecond increments, try for 5x interval time for best results.
     */
    pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
  };

  void onDisconnect(NimBLEServer *pServer) override {
    printf("Client disconnected - start advertising\n");
    NimBLEDevice::startAdvertising();
  };

  void onMTUChange(uint16_t MTU, ble_gap_conn_desc *desc) override {
    printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
  };
};

const char *SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const char *CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

void app_main(void) {
  printf("Starting BLE work!\n");
  initArduino();

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
  pService->start();

  auto pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(esp_name);
//  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setAppearance(0x0340);
  pAdvertising->setScanResponse(false);

  auto pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdCallback(pCharacteristic));
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

  NimBLEDevice::startAdvertising();
  printf("Characteristic defined! Now you can read it in your phone!\n");
}
