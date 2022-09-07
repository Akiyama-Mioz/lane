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

class Track;
class Strip;

// Maybe I should use factory pattern.
class BrightnessCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit BrightnessCharCallback(Strip &strip): strip(strip) {};
};

class StatusCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit StatusCharCallback(Strip &strip): strip(strip) {};
};

class ConfigCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit ConfigCharCallback(Strip &strip) : strip(strip) {}
};

class MaxLEDsCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit MaxLEDsCharCallback(Strip &strip) : strip(strip) {}
};

#endif //HELLO_WORLD_STRIPCALLBACKS_H
