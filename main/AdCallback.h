//
// Created by Kurosu Chan on 2022/8/4.
//
#ifndef HELLO_WORLD_ADCALLBACK_H
#define HELLO_WORLD_ADCALLBACK_H

#include "utils.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include <map>
#include "ble.pb.h"


class AdCallback : public BLEAdvertisedDeviceCallbacks {
  NimBLECharacteristic *characteristic = nullptr;
  // deviceName, payload
  std::map<std::string, std::string> lastDevices;

  /**
   * @brief callback when a device is found
   * @param advertisedDevice the device found
   * @return void
   * @note This function will use nanopb to encode the payload and then send it to the characteristic
   */
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
