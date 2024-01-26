//
// Created by Kurosu Chan on 2023/10/10.
//

#include "whitelist.h"
#include "pb_decode.h"

namespace white_list {

bool is_device_in_whitelist(const WhiteItem &item, BLEAdvertisedDevice &device) {
  return std::visit(IsDeviceVisitor{device}, item);
}

std::vector<WhiteItem> parse_white_list_response(const uint8_t *buffer, const size_t size) {
  std::vector<WhiteItem> result;
  pb_istream_t stream        = pb_istream_from_buffer(buffer, size);
  WhiteListResponse response = WhiteListResponse_init_zero;
  if (!pb_decode(&stream, WhiteListResponse_fields, &response)) {
    return result;
  }
  auto white_list_decode = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
    if (arg == nullptr) {
      return false;
    }
    auto &result = *static_cast<std::vector<WhiteItem> *>(*arg);
    ::WhiteItem item;
    std::string name{};
    std::array<uint8_t, BLE_MAC_ADDR_SIZE> addr{};
    item.item.mac.funcs.decode = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
      if (arg == nullptr) {
        return false;
      }
      auto &addr = *static_cast<std::array<uint8_t, BLE_MAC_ADDR_SIZE> *>(*arg);
      return false;
    };
    item.item.mac.arg           = &addr;
    item.item.name.funcs.decode = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
      if (arg == nullptr) {
        return false;
      }
      auto &name = *static_cast<std::string *>(*arg);
      return false;
    };
    item.item.name.arg = &name;
    switch (item.which_item) {
      case WhiteItem_name_tag: {
        auto n = Name{name};
        auto i = WhiteItem(n);
        result.emplace_back(i);
        break;
      }
      case WhiteItem_mac_tag: {
        auto m = Addr{addr};
        auto i = WhiteItem(m);
        result.emplace_back(i);
        break;
      }
      default:
        return false;
    }

    return false;
  };
  response.response.list.items.funcs.decode = white_list_decode;
  response.response.list.items.arg          = &result;
  return result;
}
}
