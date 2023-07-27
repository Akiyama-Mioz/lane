//
// Created by Kurosu Chan on 2022/8/5.
//
#include "Lane.h"

namespace lane {
void ControlCharCallback::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) {
  auto data = characteristic->getValue();
}

void ConfigCharCallback::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) {
  auto data = characteristic->getValue();
}
};
