//
// Created by Kurosu Chan on 2023/10/10.
//

#ifndef TRACK_LONG_WHITELIST_H
#define TRACK_LONG_WHITELIST_H

#include <variant>
#include <string>
#include <regex>
#include <etl/optional.h>
#include <ble.pb.h>

#ifdef SIMPLE_LOG
#include "simple_log.h"
#endif

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
using response_t = std::variant<list_t, ::WhiteListErrorCode>;
using request_t = std::variant<list_t, ::WhiteListCommand>;

bool marshal_set_white_list(pb_ostream_t *ostream, ::WhiteListSet &set, list_t &list);

bool marshal_white_list_response(pb_ostream_t *ostream, ::WhiteListResponse &response, response_t &list);

etl::optional<list_t>
unmarshal_set_white_list(pb_istream_t *istream, ::WhiteListSet &set);

etl::optional<request_t>
unmarshal_while_list_request(pb_istream_t *istream, ::WhiteListRequest &request);
}

#ifdef ESP32
#include "whitelist_esp.tpp"
#endif

#endif // TRACK_LONG_WHITELIST_H
