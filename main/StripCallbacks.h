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
class Strip;

// Maybe I should use factory pattern.
class BrightnessCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) override;

  explicit BrightnessCharCallback(Strip &strip): strip(strip) {};
};

class StatusCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) override;

  explicit StatusCharCallback(Strip &strip): strip(strip) {};
};

class ConfigCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
  int last_count = -1;
  int last_total = 0;
  size_t last_offset = 0;
  void reset();
public:
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) override;

  explicit ConfigCharCallback(Strip &strip) : strip(strip) {}
};

class OptionsCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo& connInfo) override;

  explicit OptionsCharCallback(Strip &strip) : strip(strip) {}
};

#endif //HELLO_WORLD_STRIPCALLBACKS_H
