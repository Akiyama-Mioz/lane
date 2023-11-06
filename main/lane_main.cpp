#include "inc/utils.h"
#include <vector>
#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "inc/Lane.h"
#include "inc/whitelist.h"
#include <memory.h>
#include "common.h"
#include "inc/ScanCallback.h"


using namespace common;
using namespace lane;
/**
 * @brief config the characteristic for BLE
 * @param[in] server
 * @param[out] ble the LaneBLE to be written, expected to be initialized
 * @param[in] lane
 */
void initBLE(NimBLEServer *server, LaneBLE &ble, Lane &lane) {
  if (server == nullptr) {
    return;
  }

  ble.service = server->createService(BLE_SERVICE_UUID);

  ble.ctrl_char = ble.service->createCharacteristic(BLE_CHAR_CONTROL_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  auto ctrl_cb  = new ControlCharCallback(lane);
  ble.ctrl_char->setCallbacks(ctrl_cb);

  /// write to control and read/notify for the state
  ble.config_char = ble.service->createCharacteristic(BLE_CHAR_CONFIG_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  auto config_cb  = new ConfigCharCallback(lane);
  ble.config_char->setCallbacks(config_cb);

  ble.service->start();
}

extern "C" [[noreturn]] void app_main() {
  const auto TAG = "main";
  initArduino();

  etl::array<uint8_t, PREF_WHITE_LIST_MAX_LENGTH> white_list_buf{};
  Preferences pref;
  pref.begin(PREF_RECORD_NAME, true);
  auto line_length   = pref.getFloat(PREF_LINE_LENGTH_NAME, DEFAULT_LINE_LENGTH.count());
  auto active_length = pref.getFloat(PREF_ACTIVE_LENGTH_NAME, DEFAULT_ACTIVE_LENGTH.count());
  auto line_LEDs_num = pref.getULong(PREF_LINE_LEDs_NUM_NAME, DEFAULT_LINE_LEDs_NUM);
  auto total_length  = pref.getFloat(PREF_TOTAL_LENGTH_NAME, DEFAULT_TARGET_LENGTH.count());
  auto color         = pref.getULong(PREF_COLOR_NAME, utils::Colors::Red);
  auto sz            = pref.getBytes(PREF_WHITE_LIST_NAME, white_list_buf.data(), PREF_WHITE_LIST_MAX_LENGTH);
  white_list::list_t white_list{};
  if (sz > 0) {
    auto istream              = pb_istream_from_buffer(white_list_buf.data(), sz);
    ::WhiteList pb_white_list = WhiteList_init_zero;
    auto list                 = white_list::unmarshal_white_list(&istream, pb_white_list);
    if (!list.has_value()) {
      ESP_LOGE(TAG, "Failed to unmarshal white list");
    }
    white_list = white_list::list_t{list.value()};
  } else {
    ESP_LOGE(TAG, "Failed to read white rules from flash. Skip deserialization.");
  }
  pref.end();
  if (!white_list.empty()) {
    for (auto &item : white_list) {
      if (std::holds_alternative<white_list::Addr>(item)) {
        auto addr = std::get<white_list::Addr>(item);
        ESP_LOGI(TAG, "White list addr: %02x::%02x::%02x::%02x::%02x::%02x",
                 addr.addr[0], addr.addr[1], addr.addr[2], addr.addr[3], addr.addr[4], addr.addr[5]);
      } else if (std::holds_alternative<white_list::Name>(item)) {
        auto name = std::get<white_list::Name>(item);
        ESP_LOGI(TAG, "White list name: %s", name.name.c_str());
      }
    }
  } else {
    ESP_LOGI(TAG, "White list is empty");
  }
  auto default_cfg = lane::LaneConfig{
      .color         = color,
      .line_length   = lane::meter(line_length),
      .active_length = lane::meter(active_length),
      .total_length  = lane::meter(total_length),
      .line_LEDs_num = line_LEDs_num,
      .fps           = DEFAULT_FPS,
  };

  NimBLEDevice::init(BLE_NAME);
  auto &server = *NimBLEDevice::createServer();
  server.setCallbacks(new ServerCallbacks());

  /**** Initialize NeoPixel. ****/
  /** an aux function used to let FreeRTOS do it work.
   * since FreeRTOS is implemented in C where we can't have lambda capture, so pStrip must be
   * passed as parameter.
   **/
  auto lane_loop = [](void *param) {
    auto &lane = *static_cast<Lane *>(param);
    lane.loop();
  };
  // using singleton pattern to avoid memory leak
  auto s     = strip::AdafruitPixel(default_cfg.line_LEDs_num, LED_PIN, strip::AdafruitPixel::default_pixel_type);
  auto &lane = *Lane::get();
  lane.setStrip(std::make_unique<decltype(s)>(std::move(s)));
  auto lane_ble = LaneBLE();
  initBLE(&server, lane_ble, lane);
  lane.setBLE(lane_ble);
  lane.setConfig(default_cfg);
  ESP_ERROR_CHECK(lane.begin());

  /************** HR char initialization ****************/

  auto &hr_service      = *server.createService(BLE_CHAR_HR_SERVICE_UUID);
  auto &hr_char         = *hr_service.createCharacteristic(BLE_CHAR_HEARTBEAT_UUID,
                                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  auto pScanCb          = new ScanCallback(&hr_char);
  auto &white_list_char = *hr_service.createCharacteristic(BLE_CHAR_WHITE_LIST_UUID,
                                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  auto &device_char     = *hr_service.createCharacteristic(BLE_CHAR_DEVICE_UUID,
                                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  auto pWhiteListCb     = new WhiteListCallback();
  white_list_char.setCallbacks(pWhiteListCb);
  pScanCb->onResultCb = [&device_char](std::string device_name, const uint8_t *addr) {
    uint8_t buf[32]                 = {0};
    auto TAG                        = "onResultCb";
    auto ostream                    = pb_ostream_from_buffer(buf, sizeof(buf));
    ::bluetooth_device_pb device_pb = bluetooth_device_pb_init_zero;
    device_pb.mac.funcs.encode      = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
      const auto addr_ptr = reinterpret_cast<const uint8_t *>(*arg);
      if (!pb_encode_tag_for_field(stream, field)) {
        return false;
      }
      return pb_encode_string(stream, addr_ptr, BLE_MAC_ADDR_SIZE);
    };
    device_pb.mac.arg           = const_cast<uint8_t *>(addr);
    const auto target_name_size = sizeof(device_pb.name) - 1;
    if (device_name.size() > target_name_size) {
      device_name.resize(target_name_size);
      ESP_LOGW(TAG, "Truncated device name to %s", device_name.c_str());
    }
    std::memcpy(device_pb.name, device_name.c_str(), device_name.size());
    auto ok = pb_encode(&ostream, bluetooth_device_pb_fields, &device_pb);
    if (!ok) {
      ESP_LOGE(TAG, "Failed to encode the device");
      return;
    }
    auto sz = ostream.bytes_written;
    device_char.setValue(buf, sz);
    device_char.notify();
  };
  pWhiteListCb->setList = [pScanCb](white_list::list_t list) {
    const auto TAG = "setList";
    pScanCb->set_white_list(list);
    uint8_t buf[ScanCallback::MAX_OSTREAM_SIZE]{0};
    auto ostream           = pb_ostream_from_buffer(buf, sizeof(buf));
    ::WhiteList white_list = WhiteList_init_zero;
    auto ok                = white_list::marshal_white_list(&ostream, white_list, list);
    if (!ok) {
      ESP_LOGE(TAG, "Failed to marshal white list");
      return;
    }
    Preferences pref;
    pref.begin(PREF_RECORD_NAME, false);
    pref.putBytes(PREF_WHITE_LIST_NAME, buf, ostream.bytes_written);
    pref.end();
  };
  pWhiteListCb->getList = [pScanCb]() {
    auto list = pScanCb->white_list();
    return list;
  };
  hr_service.start();

  auto &ad = *NimBLEDevice::getAdvertising();
  ad.setName(BLE_NAME);
  ad.setScanResponse(false);

  // https://github.com/espressif/esp-idf/issues/11651
  // https://lang-ship.com/reference/unofficial/M5StickC/Functions/freertos/task/
  xTaskCreatePinnedToCore(lane_loop,
                          "lane", 5120,
                          &lane, configMAX_PRIORITIES - 3,
                          nullptr, 1);

  server.start();
  NimBLEDevice::startAdvertising();
  auto &scan = *NimBLEDevice::getScan();
  scan.setScanCallbacks(pScanCb);
  scan.setInterval(1349);
  scan.setWindow(449);
  scan.setActiveScan(true);
  constexpr auto scanTime = std::chrono::milliseconds(750);
  // scan time + sleep time
  constexpr auto scanTotalTime = std::chrono::milliseconds(1000);
  static_assert(scanTotalTime > scanTime);
  ESP_LOGI(TAG, "Initiated");
  pref.end();
  for (;;) {
    bool ok = scan.start(scanTime.count(), false);
    if (!ok) {
      ESP_LOGE(TAG, "Failed to start scan");
    }
    vTaskDelay(scanTotalTime.count() / portTICK_PERIOD_MS);
  }
}
