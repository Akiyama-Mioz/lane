//
// Created by Kurosu Chan on 2022/8/5.
//

#ifndef HELLO_WORLD_STRIPCALLBACKS_H
#define HELLO_WORLD_STRIPCALLBACKS_H

#include <Adafruit_NeoPixel.h>
#include "NimBLEDevice.h"
#include <map>
#include <iostream>
#include <vector>
#include "pb_encode.h"
#include "pb_decode.h"
#include "msg.pb.h"
#include "utils.h"


class Strip;
// Maybe I should use factory pattern.
class ColorCharCallback : public NimBLECharacteristicCallbacks {
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit ColorCharCallback(Strip &strip);
};

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
class SpeedCustomCharCallback : public NimBLECharacteristicCallbacks {
  ValueRetriever<float> *cur_speedCustom;
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit SpeedCustomCharCallback(Strip &strip, ValueRetriever<float> *speedCustom):cur_speedCustom(speedCustom), strip(strip){}
};

class SpeedZeroCharCallback : public NimBLECharacteristicCallbacks {
  ValueRetriever<float> *cur_speed0;
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit SpeedZeroCharCallback(Strip &strip, ValueRetriever<float> *speed0):cur_speed0(speed0), strip(strip){}
};
class SpeedOneCharCallback : public NimBLECharacteristicCallbacks {
  ValueRetriever<float> *cur_speed1;
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit SpeedOneCharCallback(Strip &strip, ValueRetriever<float> *speed1):cur_speed1(speed1), strip(strip){}
};
class SpeedTwoCharCallback : public NimBLECharacteristicCallbacks {
  ValueRetriever<float> *cur_speed2;
  Strip &strip;
public:
  void onWrite(NimBLECharacteristic *characteristic) override;

  explicit SpeedTwoCharCallback(Strip &strip, ValueRetriever<float> *speed2):cur_speed2(speed2), strip(strip){}
};

#endif //HELLO_WORLD_STRIPCALLBACKS_H
