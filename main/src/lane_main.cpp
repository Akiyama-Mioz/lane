#include "utils.h"
#include <vector>
#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "NimBLEDevice.h"
#include "Lane.h"
#include <etl/map.h>
#include "whitelist.h"
#include <memory.h>
#include "common.h"
#include "ScanCallback.h"
#include "src/RadioLib.h"
#include <mutex>
#include "hr_lora.h"
#include "EspHal.h"

void *rf_receive_data = nullptr;
struct rf_receive_data_t {
  EventGroupHandle_t evt_grp = nullptr;
};

constexpr auto RecvEvt = BIT0;

constexpr auto MAX_DEVICE_COUNT = 16;
using device_name_map_t         = etl::map<int, std::string, MAX_DEVICE_COUNT>;

struct handle_message_callbacks {
  std::function<std::optional<std::string>(int)> get_device;
  std::function<void(etl::pair<int, std::string>)> set_device;
};

void handle_message(uint8_t *pdata, size_t size, const handle_message_callbacks &callbacks) {
  auto magic = pdata[0];
  switch (magic) {
    case HrLoRa::hr_data::magic: {
      auto p_hr_data = HrLoRa::hr_data::unmarshal(pdata, size);
      if (p_hr_data) {
        auto p_name = callbacks.get_device(p_hr_data->key);
        if (!p_name) {
          return;
        }
      }
      break;
    }
    case HrLoRa::query_device_by_mac_response::magic: {
      auto p_response = HrLoRa::query_device_by_mac::unmarshal(pdata, size);
      if (p_response){

      }
      break;
    }
    case HrLoRa::query_device_by_mac::magic:
    case HrLoRa::set_name_map_key::magic: {
      break;
    }
    default: {
      ESP_LOGW("recv", "unknown magic: %d", magic);
    }
  }
}

/**
 * @brief try to transmit the data
 * @note would block until the transmission is done and will start receiving after that
 */
void try_transmit(uint8_t *data, size_t size,
                  SemaphoreHandle_t lk, TickType_t timeout_tick,
                  LLCC68 &rf) {
  const auto TAG = "try_transmit";
  if (xSemaphoreTake(lk, timeout_tick) != pdTRUE) {
    ESP_LOGE(TAG, "failed to take rf_lock; no transmission happens;");
    return;
  }
  auto err = rf.transmit(data, size);
  if (err == RADIOLIB_ERR_NONE) {
    // ok
  } else if (err == RADIOLIB_ERR_TX_TIMEOUT) {
    ESP_LOGW(TAG, "tx timeout; please check the busy pin;");
  } else {
    ESP_LOGE(TAG, "failed to transmit, code %d", err);
  }
  rf.standby();
  rf.startReceive();
  xSemaphoreGive(lk);
}

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

extern "C" void app_main();

void app_main() {
  const auto TAG = "main";
  initArduino();

  Preferences pref;
  pref.begin(PREF_RECORD_NAME, true);
  auto line_length   = pref.getFloat(PREF_LINE_LENGTH_NAME, DEFAULT_LINE_LENGTH.count());
  auto active_length = pref.getFloat(PREF_ACTIVE_LENGTH_NAME, DEFAULT_ACTIVE_LENGTH.count());
  auto line_LEDs_num = pref.getULong(PREF_LINE_LEDs_NUM_NAME, DEFAULT_LINE_LEDs_NUM);
  auto total_length  = pref.getFloat(PREF_TOTAL_LENGTH_NAME, DEFAULT_TARGET_LENGTH.count());
  auto color         = pref.getULong(PREF_COLOR_NAME, utils::Colors::Red);
  auto default_cfg   = lane::LaneConfig{
        .color         = color,
        .line_length   = lane::meter(line_length),
        .active_length = lane::meter(active_length),
        .total_length  = lane::meter(total_length),
        .line_LEDs_num = line_LEDs_num,
        .fps           = DEFAULT_FPS,
  };
  pref.end();

  static auto hal    = EspHal(pin::SCK, pin::MISO, pin::MOSI);
  static auto module = Module(&hal, pin::NSS, pin::DIO1, pin::LoRa_RST, pin::BUSY);
  auto *rf_lock      = xSemaphoreCreateMutex();
  if (rf_lock == nullptr) {
    ESP_LOGE("rf", "failed to create rf_lock");
    esp_restart();
  }
  static auto rf = LLCC68(&module);
  auto st        = rf.begin(434, 500.0, 7, 7,
                            RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 22, 8, 1.6);
  if (st != RADIOLIB_ERR_NONE) {
    ESP_LOGE("rf", "failed, code %d", st);
    esp_restart();
  }

  /********* recv interrupt initialization *********/
  auto evt_grp    = xEventGroupCreate();
  auto *data      = new rf_receive_data_t{.evt_grp = evt_grp};
  rf_receive_data = data;
  rf.setPacketReceivedAction([]() {
    auto *data            = static_cast<rf_receive_data_t *>(rf_receive_data);
    BaseType_t task_woken = pdFALSE;
    if (data->evt_grp != nullptr) {
      // https://github.com/espressif/esp-idf/issues/5897
      // https://github.com/espressif/esp-idf/pull/6692
      xEventGroupSetBits(data->evt_grp, RecvEvt);
    }
  });
  /********* end of recv interrupt initialization *********/

  /********* recv task initialization            *********/
  auto recv_task = [evt_grp, rf_lock](LLCC68 &rf) {
    const auto TAG = "recv";
    for (;;) {
      xEventGroupWaitBits(evt_grp, RecvEvt, pdTRUE, pdFALSE, portMAX_DELAY);
      uint8_t data[255];
      // https://www.freertos.org/a00122.html
      if (xSemaphoreTake(rf_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "failed to take rf_lock");
        abort();
      }
      size_t size = rf.receive(data, sizeof(data));
      xSemaphoreGive(rf_lock);
      if (size == 0) {
        ESP_LOGW(TAG, "empty data");
      }
      ESP_LOGI(TAG, "recv=%s", utils::toHex(data, size).c_str());
      // TODO: handle message stuff
      // handle_message(data, size, handle_message_callbacks);
    }
  };

  struct recv_task_param_t {
    std::function<void(LLCC68 &)> task;
    LLCC68 *rf;
    TaskHandle_t handle;
    EventGroupHandle_t evt_grp;
  };

  /**
   * a helper function to run a function on a new FreeRTOS task
   */
  auto run_recv_task = [](void *pvParameter) {
    auto param = reinterpret_cast<recv_task_param_t *>(pvParameter);
    [[unlikely]] if (param->task != nullptr && param->rf != nullptr) {
      param->task(*param->rf);
    } else {
      ESP_LOGW("recv task", "bad precondition");
    }
    auto handle = param->handle;
    delete param;
    vTaskDelete(handle);
  };
  static auto recv_param = recv_task_param_t{recv_task, &rf, nullptr, evt_grp};
  xTaskCreate(run_recv_task,
              "recv", 4096,
              &recv_param, 1,
              &recv_param.handle);
  /********** end of recv task initialization **********/

  ESP_LOGI(TAG, "LoRa RF initiated");

  /********* lane initialization *********/
  auto lane_task = [](void *param) {
    auto &lane = *static_cast<Lane *>(param);
    lane.loop();
  };
  auto s     = strip::AdafruitPixel(default_cfg.line_LEDs_num, pin::LED, strip::AdafruitPixel::default_pixel_type);
  auto &lane = Lane::get();
  lane.setStrip(std::make_unique<decltype(s)>(std::move(s)));
  auto lane_ble = LaneBLE{};
  /********* end of lane initialization *********/

  /********* BLE initialization *********/
  NimBLEDevice::init(BLE_NAME);
  auto &server = *NimBLEDevice::createServer();
  server.setCallbacks(new ServerCallbacks());

  initBLE(&server, lane_ble, lane);
  lane.setBLE(lane_ble);
  lane.setConfig(default_cfg);
  ESP_ERROR_CHECK(lane.begin());

  auto &hr_service = *server.createService(BLE_CHAR_HR_SERVICE_UUID);
  auto &hr_char    = *hr_service.createCharacteristic(BLE_CHAR_HEARTBEAT_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  hr_service.start();

  auto &ad = *NimBLEDevice::getAdvertising();
  ad.setName(BLE_NAME);
  ad.setScanResponse(false);

  // intend to give it higher priority
  xTaskCreatePinnedToCore(lane_task,
                          "lane", 5120,
                          &lane, configMAX_PRIORITIES - 4,
                          nullptr, 1);

  server.start();
  NimBLEDevice::startAdvertising();
  ESP_LOGI(TAG, "Initiated");
  vTaskDelete(nullptr);
}
