//
// Created by Kurosu Chan on 2023/11/6.
//

#ifndef TRACK_SHORT_COMMON_H
#define TRACK_SHORT_COMMON_H

namespace common {
static auto BLE_NAME          = "lane-011";
static const uint16_t LED_PIN = 23;
// static const uint16_t LED_PIN = 2;

static const char *BLE_SERVICE_UUID         = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
static const char *BLE_CHAR_HR_SERVICE_UUID = "180d";
static const char *BLE_CHAR_CONTROL_UUID    = "24207642-0d98-40cd-84bb-910008579114";
static const char *BLE_CHAR_CONFIG_UUID     = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
static const char *BLE_CHAR_HEARTBEAT_UUID  = "048b8928-d0a5-43e2-ada9-b925ec62ba27";
static const char *BLE_CHAR_WHITE_LIST_UUID = "12a481f0-9384-413d-b002-f8660566d3b0";
static const char *BLE_CHAR_DEVICE_UUID     = "a2f05114-fdb6-4549-ae2a-845b4be1ac48";
};

#endif // TRACK_SHORT_COMMON_H
