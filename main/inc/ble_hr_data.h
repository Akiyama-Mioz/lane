//
// Created by Kurosu Chan on 2023/11/7.
//

#ifndef TRACK_SHORT_BLE_HR_DATA_H
#define TRACK_SHORT_BLE_HR_DATA_H
#include <array>
#include <string>
#include <etl/optional.h>

namespace ble {
static const auto BLE_ADDR_SIZE  = 6;
using addr_t                  = std::array<uint8_t, BLE_ADDR_SIZE>;
struct hr_data{
  struct t {
    std::string name;
    std::array<uint8_t, 6> addr;
  };
  static size_t size_needed(const t &data) {
    return BLE_ADDR_SIZE + 1 + data.name.size();
  }
  static etl::optional<t> unmarshal(const uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < BLE_ADDR_SIZE) {
      return etl::nullopt;
    }
    t data;
    for (int i = 0; i < BLE_ADDR_SIZE; ++i) {
      data.addr[i] = buffer[i];
    }
    size_t sz = buffer[BLE_ADDR_SIZE];

    return data;
  }
};
}

#endif // TRACK_SHORT_BLE_HR_DATA_H
