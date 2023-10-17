//
// Created by Kurosu Chan on 2022/8/4.
//
#ifndef HELLO_WORLD_ADCALLBACK_H
#define HELLO_WORLD_ADCALLBACK_H

#include "utils.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "whitelist.h"
#include <map>
#include <etl/flat_map.h>
#include <etl/vector.h>

const int BLE_MAC_ADDR_SIZE = white_list::BLE_MAC_ADDR_SIZE;
using DeviceAddr            = etl::array<uint8_t, BLE_MAC_ADDR_SIZE>;

struct DeviceInfo {
  DeviceAddr address;
  NimBLEClient *client;
};

/// Should not touch the info pointer (Read Only)
using DeviceMap = etl::vector<DeviceInfo, 24>;

class ScanCallback : public NimBLEScanCallbacks {
  // the characteristic to send the heart rate data to the client with the format described in
  // `hr_data.ksy`
public:
  white_list::list_t white_list;

private:
  DeviceMap devices{};
  NimBLECharacteristic *hr_char = nullptr;

  /**
   * @brief callback when a device is found
   * @param advertisedDevice the device found
   * @return void
   * @note This function will use nanopb to encode the payload and then send it to the characteristic
   */
  void onResult(BLEAdvertisedDevice *advertisedDevice) override;
  void handleHrAdvertised(BLEAdvertisedDevice *advertisedDevice);
  void handleHrWhiteListConnection(BLEAdvertisedDevice *advertisedDevice);

public:
  explicit ScanCallback(NimBLECharacteristic *c) : hr_char(c) {}
  DeviceMap &getDevices() { return devices; }
};

class HRClientCallbacks : public NimBLEClientCallbacks {
  DeviceMap *devices;
  DeviceAddr addr;

  void onDisconnect(NimBLEClient *pClient, int reason) override;

public:
  explicit HRClientCallbacks(DeviceMap *d, DeviceAddr addr) : devices(d), addr(addr) {}
  DeviceMap &getDevices() { return *devices; }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override;

  void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override;
};

class WhiteListCallback : public NimBLECharacteristicCallbacks {
public:
  using set_list_fn           = std::function<void(white_list::list_t)>;
  using get_list_fn          = std::function<const white_list::list_t &(void)>;
  set_list_fn setList         = nullptr;
  get_list_fn getList = nullptr;
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;
};

#endif // HELLO_WORLD_ADCALLBACK_H
