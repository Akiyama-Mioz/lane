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
#include "msg.pb.h"


class Track;
class Strip;

// Maybe I should use factory pattern.
class BrightnessCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit BrightnessCharCallback(Strip &strip);
};

class DelayCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit DelayCharCallback(Strip &strip);
};

class MaxLEDsCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit MaxLEDsCharCallback(Strip &strip);
};

class StatusCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit StatusCharCallback(Strip &strip);
};

class HaltDelayCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit HaltDelayCharCallback(Strip &strip);
};

class SpeedCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
  Track &track;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit SpeedCharCallback(Strip &strip, Track &track) : strip(strip), track(track) {}
};

#endif //HELLO_WORLD_STRIPCALLBACKS_H
