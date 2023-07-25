//
// Created by Kurosu Chan on 2022/8/4.
//
#ifndef HELLO_WORLD_ADCALLBACK_H
#define HELLO_WORLD_ADCALLBACK_H

#include "utils.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include <map>
#include <etl/flat_map.h>
#include <etl/vector.h>
#include "ble.pb.h"

struct DeviceInfo {
  BLEClient* client;
  int64_t last_seen;
  uint8_t last_hr;
};

/// Should not touch the info pointer (Read Only)
using DeviceMap = etl::flat_map<uint16_t, DeviceInfo*, 10>;

class ScanCallback : public BLEAdvertisedDeviceCallbacks {
  NimBLECharacteristic *characteristic = nullptr;
  DeviceMap devices;

  /**
   * @brief callback when a device is found
   * @param advertisedDevice the device found
   * @return void
   * @note This function will use nanopb to encode the payload and then send it to the characteristic
   */
  void onResult(BLEAdvertisedDevice *advertisedDevice) override;

public:
  explicit ScanCallback(NimBLECharacteristic *c) : characteristic(c) {}
  DeviceMap& getDevices() { return devices; }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo) override;

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo, int reason) override;

  void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override;
};


#endif //HELLO_WORLD_ADCALLBACK_H
