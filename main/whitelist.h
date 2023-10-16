//
// Created by Kurosu Chan on 2023/10/10.
//

#ifndef TRACK_LONG_WHITELIST_H
#define TRACK_LONG_WHITELIST_H

#include <variant>
#include <string>
#include "NimBLEDevice.h"
#include <regex>
#include <ble.pb.h>

namespace white_list {
const auto BLE_MAC_ADDR_SIZE = 6;
struct Name {
  std::string name;
};

struct Addr {
  std::array<uint8_t, BLE_MAC_ADDR_SIZE> addr;
};

using item_t = std::variant<Name, Addr>;
using list_t = std::vector<item_t>;
using response_t = std::variant<list_t, ::BlueScanErrorCode>;

// https://stackoverflow.com/questions/75278137/correct-use-of-stdvariant-and-stdvisit-when-functor-requires-multiple-argume
struct IsDeviceVisitor {
  BLEAdvertisedDevice &device;
  bool operator()(const white_list::Name &name) const {
    const auto device_name = device.getName();
    if (device_name.empty()) {
      return false;
    }
    auto re = std::regex(name.name);
    return std::regex_match(device_name, re);
  }

  bool operator()(const white_list::Addr &addr) const {
    // a pointer to the uint8_t[6] array of the address
    const auto device_addr = device.getAddress().getNative();
    return std::memcmp(addr.addr.data(), device_addr, BLE_MAC_ADDR_SIZE) == 0;
  }
};

bool is_device_in_whitelist(const item_t &item, BLEAdvertisedDevice &device);
}

#endif // TRACK_LONG_WHITELIST_H
