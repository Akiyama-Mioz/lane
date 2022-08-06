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



/**
 * @brief A auxiliary class to handle the callback of nanopb encoding.
 * @warning: T must be a pointer type (with a little star behind it)
 */
template<class T>
struct PbCallback {
  using PbEncodeCallback = bool (*)(pb_ostream_t *stream, const pb_field_t *field, T const *arg);
  using PbEncodeCallbackVoid = bool (*)(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
  /**
   * @brief The parameter should be passed to the func with pointer type T.
   */
  T arg;
  /**
   * @brief The function to be called when encoding.
   */
  PbEncodeCallback func;
  /**
   * @brief cast the function to the type of PbEncodeCallbackVoid.
   * @return a function pointer of bool (*)(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
   */
  PbEncodeCallbackVoid func_to_void() {
    return reinterpret_cast<PbEncodeCallbackVoid>(*func);
  }
  /**
   * @brief cast the arg to void *.
   * @return void *
   */
  void * arg_to_void() {
    return std::addressof(arg);
  }
  /**
   * @note constructor is deleted. use the struct initializer instead.
   * @see <a href="https://en.cppreference.com/w/c/language/struct_initialization">Struct Initialization</a>
   */
  PbCallback() = delete;
};

class AdCallback : public BLEAdvertisedDeviceCallbacks {
  NimBLECharacteristic *characteristic = nullptr;
  // deviceName, payload
  std::map<std::string, std::string> lastDevices;

  /**
   * @brief callback when a device is found
   * @param advertisedDevice - the device found
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
