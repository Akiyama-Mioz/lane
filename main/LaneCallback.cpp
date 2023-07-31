//
// Created by Kurosu Chan on 2023/7/31.
//

#include "Lane.h"

//****************************** Callback ************************************/

namespace lane {
void ControlCharCallback::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) {
  auto data                 = characteristic->getValue();
  ::LaneControl control_msg = LaneControl_init_zero;
  auto ostream              = pb_istream_from_buffer(data.data(), data.size());
  auto ok                   = pb_decode(&ostream, LaneControl_fields, &control_msg);
  if (!ok) {
    ESP_LOGE("LANE", "Failed to decode the control message");
    return;
  }
  switch (control_msg.which_msg) {
    case LaneControl_set_speed_tag:
      lane.setSpeed(control_msg.msg.set_speed.speed);
      break;
    case LaneControl_set_status_tag:
      lane.setStatus(control_msg.msg.set_status.status);
      break;
  }
}

void ConfigCharCallback::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) {
  auto data               = characteristic->getValue();
  ::LaneConfig config_msg = LaneConfig_init_zero;
  auto ostream            = pb_istream_from_buffer(data.data(), data.size());
  auto ok                 = pb_decode(&ostream, LaneConfig_fields, &config_msg);
  if (!ok) {
    ESP_LOGE("LANE", "Failed to decode the config message");
    return;
  }
  lane.pref.begin(PREF_RECORD_NAME, false);
  switch (config_msg.which_msg) {
    case LaneConfig_color_cfg_tag:
      lane.pref.putULong(PREF_COLOR_NAME, config_msg.msg.color_cfg.rgb);
      lane.setColor(config_msg.msg.color_cfg.rgb);
      break;
    case LaneConfig_length_cfg_tag: {
      lane.pref.putFloat(PREF_LINE_LENGTH_NAME, config_msg.msg.length_cfg.line_length_m);
      lane.pref.putFloat(PREF_ACTIVE_LENGTH_NAME, config_msg.msg.length_cfg.active_length_m);
      lane.pref.putFloat(PREF_TOTAL_LENGTH_NAME, config_msg.msg.length_cfg.total_length_m);
      lane.pref.putULong(PREF_LINE_LEDs_NUM_NAME, config_msg.msg.length_cfg.line_leds_num);
      lane.cfg.line_length   = meter(config_msg.msg.length_cfg.line_length_m);
      lane.cfg.active_length = meter(config_msg.msg.length_cfg.active_length_m);
      lane.cfg.total_length  = meter(config_msg.msg.length_cfg.total_length_m);
      lane.cfg.line_LEDs_num = config_msg.msg.length_cfg.line_leds_num;
      break;
    }
  }
  lane.pref.end();
}
void ConfigCharCallback::onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) {
  const size_t size = LaneConfigRO_size;
  uint8_t data[size];
  ::LaneConfigRO config_msg             = LaneConfigRO_init_zero;
  auto ostream                          = pb_ostream_from_buffer(data, size);
  config_msg.length_cfg.line_length_m   = lane.cfg.line_length.count();
  config_msg.length_cfg.active_length_m = lane.cfg.active_length.count();
  config_msg.length_cfg.total_length_m  = lane.cfg.total_length.count();
  config_msg.length_cfg.line_leds_num   = lane.cfg.line_LEDs_num;
  config_msg.color_cfg.rgb              = lane.cfg.color;
  auto ok                               = pb_encode(&ostream, LaneConfigRO_fields, &config_msg);
  if (!ok) {
    ESP_LOGE("LANE", "Failed to encode the config message");
    return;
  }
  pCharacteristic->setValue(data, ostream.bytes_written);
}
};
