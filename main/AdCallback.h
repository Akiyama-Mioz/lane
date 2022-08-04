//
// Created by Kurosu Chan on 2022/8/4.
//
// necessary for using fmt library
#define FMT_HEADER_ONLY

#ifndef HELLO_WORLD_ADCALLBACK_H
#define HELLO_WORLD_ADCALLBACK_H

#include "fmt/core.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "utils.h"
#include <map>

#include <pb_encode.h>
#include "ble.pb.h"

class AdCallback : public BLEAdvertisedDeviceCallbacks {
  NimBLECharacteristic *characteristic = nullptr;
// deviceName, payload
  std::map<std::string, std::string> lastDevices;

  void onResult(BLEAdvertisedDevice *advertisedDevice) override;

public:
  explicit AdCallback(NimBLECharacteristic *c) : characteristic(c) {}
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer) override;;

  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) override;;

  void onDisconnect(NimBLEServer *pServer) override;;

  void onMTUChange(uint16_t MTU, ble_gap_conn_desc *desc) override;;
};


#endif //HELLO_WORLD_ADCALLBACK_H
