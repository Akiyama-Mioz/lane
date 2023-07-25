//
// Created by Kurosu Chan on 2022/8/5.
//

#ifndef HELLO_WORLD_STRIPCALLBACKS_H
#define HELLO_WORLD_STRIPCALLBACKS_H

#include "utils.h"
#include <Adafruit_NeoPixel.h>
#include "NimBLEDevice.h"
#include <map>
#include <iostream>
#include <vector>
#include "pb_encode.h"
#include "pb_decode.h"
#include "track_config.pb.h"
#include "track_option.pb.h"

class Track;
class Lane;

class ControlCharCallback : public NimBLECharacteristicCallbacks {
  Lane &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) override;

  explicit ControlCharCallback(Lane &strip): strip(strip) {};
};

class ConfigCharCallback : public NimBLECharacteristicCallbacks {
  Lane &lane;
  int last_count = -1;
  int last_total = 0;
  size_t last_offset = 0;
  void reset();
public:
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) override;

  explicit ConfigCharCallback(Lane &strip) : lane(strip) {}
};

#endif //HELLO_WORLD_STRIPCALLBACKS_H
