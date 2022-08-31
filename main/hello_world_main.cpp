// necessary for using fmt library
#define FMT_HEADER_ONLY

// fmt library should be included first
#include "utils.h"
#include <cstdio>
#include <vector>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include <map>
#include <utils.h>
#include <iostream>
#include "pb_encode.h"
#include "pb_decode.h"
#include "msg.pb.h"
//#include "pb.h"
// #include "nanopb_cpp.h"
#include "StripCommon.h"
#include "AdCallback.h"
#define TEST(x) \
    if (!(x)) { \
        fprintf(stderr, "\033[31;1mFAILED:\033[22;39m %s:%d %s\n", __FILE__, __LINE__, #x); \
        status = 1; \
    } else { \
        printf("\033[32;1mOK:\033[22;39m %s\n", #x); \
    }
extern "C" { void app_main(); }

static auto esp_name = "e-track 011";
class Strip;
//In seconds
static const int scanTime = 1;
static const int scanInterval = 50;



[[noreturn]]
void scanTask(BLEScan *pBLEScan) {
  for (;;) {
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    printf("Devices found: %d\n", foundDevices.getCount());
    printf("Scan done!\n");
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
    vTaskDelay(scanInterval / portTICK_PERIOD_MS); // Delay a second between loops.
  }

  vTaskDelete(nullptr);
}

uint8_t buf[50];
uint8_t  message_length =40;
auto dists = std::vector<int>{100, 200, 300, 400, 500, 600, 700, 800};
auto speedes = std::vector<float>{2, 2.5, 3, 4, 3.5, 3, 2, 4};
int nanopb_test(){
  int status = 0;
 TrackConfig config_decoded = TrackConfig_init_zero;
  auto recevied_tuple = std::vector<TupleIntFloat>{};
  pb_istream_t istream = pb_istream_from_buffer(buf, message_length);
  TrackConfig config_decoded = TrackConfig_init_zero;
auto decode_tuple_list = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
    // Things will be weird if you dereference it
    // I don't know why though
    // auto tuple_list = *(reinterpret_cast<std::vector<TupleIntFloat> *>(*arg)); // not ok
    // https://stackoverflow.com/questions/1910712/dereference-vector-pointer-to-access-element
    // auto *tuple_list = (reinterpret_cast<std::vector<TupleIntFloat> *>(*arg)); // ok as pointer
    auto &tuple_list = *(reinterpret_cast<std::vector<TupleIntFloat> *>(*arg)); // ok as reference
    TupleIntFloat t = TupleIntFloat_init_zero;
    bool status = pb_decode(stream, TupleIntFloat_fields, &t);
    if (status) {
      tuple_list.emplace_back(t);
      return true;
    } else {
      return false;
    }
  };
  config_decoded.lst.arg = reinterpret_cast<void *>(&recevied_tuple);
  config_decoded.lst.funcs.decode = decode_tuple_list;
  bool status_decode = pb_decode(&istream, TrackConfig_fields, &config_decoded);
  std::cout << "Decode success? " << (status_decode ? "true" : "false") << "\n";
  if (status_decode){
    if (recevied_tuple.empty()){
      std::cout << "But I got nothing! \n";
      return 1;
    }
    for (auto &t : recevied_tuple){
      std::cout << "dist: " << t.dist << "\tspeed: " << t.speed << "\n";
    }
  } else {
    std::cout << "Error: Something goes wrong when decoding \n";
    return 1;
  }

auto m = std::map<int, float>{};
for (auto &t : recevied_tuple){
      dists.emplace_back(t.dist);
      m.insert_or_assign(t.dist, t.speed);
    }
    auto res = retriever.retrieve(val);
}


const char * SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const char * CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

void app_main(void) {
  const int DEFAULT_NUM_LEDS = 4000;
  const uint16_t LED_PIN = 14;
  initArduino();

  Preferences pref;
  pref.begin("record", true);
  auto max_leds = pref.getInt("max_leds", DEFAULT_NUM_LEDS);
  
  auto brightness = pref.getUChar("brightness", 32);
  printf("max_leds stored in pref: %d\n", max_leds);
  
  printf("brightness stored in pref: %d\n", brightness);

  NimBLEDevice::init(esp_name);
  auto pServer = NimBLEDevice::createServer();
  auto pService = pServer->createService(SERVICE_UUID);
  auto pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::NOTIFY
  );
  pServer->setCallbacks(new ServerCallbacks());
  pCharacteristic->setValue(std::string{0x00});

  /**** Initialize NeoPixel. ****/
  /** an aux function used to let FreeRTOS do it work.
   * since FreeRTOS is implemented in C where we can't have lambda capture, so pStrip must be
   * passed as parameter.
  **/
  auto pFunc = [](Strip *pStrip){
    pStrip->stripTask();
  };
  // using singleton pattern to avoid memory leak
  auto pStrip = Strip::get();
  pStrip->begin(max_leds, LED_PIN, brightness);
  pStrip->initBLE(pServer);
  // Ad service must start after Strip service? why?


  auto pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(esp_name);
  pAdvertising->setAppearance(0x0340);
  pAdvertising->setScanResponse(false);

  // Initialize BLE Scanner.
  auto pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdCallback(pCharacteristic));
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setDuplicateFilter(false); // maybe I should enable it
  pBLEScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
  pBLEScan->setInterval(100);
  // Active scan time
  // less or equal setInterval value
  pBLEScan->setWindow(99);

  // xTaskCreate(reinterpret_cast<TaskFunction_t>(scanTask),
  //             "scanTask", 5000,
  //             static_cast<void *>(pBLEScan), 1,
  //             nullptr);
  xTaskCreate(reinterpret_cast<TaskFunction_t>(*pFunc),
              "stripTask", 5000,
              pStrip, 2,
              nullptr);

  NimBLEDevice::startAdvertising();
  printf("Characteristic defined! Now you can read it in your phone!\n");
  pref.end();
}

