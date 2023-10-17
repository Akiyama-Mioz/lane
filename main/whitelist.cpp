//
// Created by Kurosu Chan on 2023/10/10.
//

#include "whitelist.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include <functional>
#include <iostream>

#ifdef ESP32
#define LOG_ERR(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#elif defined(SIMPLE_LOG)
#define LOG_ERR(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
#else
#define LOG_ERR(tag, fmt, ...) // Define an empty macro if none of the conditions are met
#endif

#ifdef ESP32
#define LOG_INFO(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#elif defined(SIMPLE_LOG)
#define LOG_INFO(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(tag, fmt, ...) // Define an empty macro if none of the conditions are met
#endif

namespace white_list {
// decode one
using addr_f = std::function<bool(Addr)>;
using name_f = std::function<bool(Name)>;

// well. field item is a union, so you just only have one field set
void set_decode_white_item_addr(::WhiteItem &item, const addr_f &write_addr) {
  item.item.mac.funcs.decode = [](pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
    if (arg == nullptr) {
      return false;
    }
    auto addr = Addr{};
    // Use pb_read instead, length of the string is available in stream->bytes_left.
    // https://jpa.kapsi.fi/nanopb/docs/concepts.html#decoding-callbacks
    // https://github.com/nanopb/nanopb/blob/09234696e0ef821432a8541b950e8866f0c61f8c/tests/callbacks/decode_callbacks.c#L10
    // https://en.cppreference.com/w/cpp/algorithm/swap
    // https://github.com/nanopb/nanopb/blob/master/tests/oneof_callback/decode_oneof.c
    if (stream->bytes_left < BLE_MAC_ADDR_SIZE) {
      LOG_ERR("white_list", "field length is not enough for bluetooth address. expected: %d, actual: %d", BLE_MAC_ADDR_SIZE, static_cast<int>(stream->bytes_left));
      return false;
    }

    const auto &w = *reinterpret_cast<addr_f *>(*arg);
    if (!pb_read(stream, addr.addr.data(), BLE_MAC_ADDR_SIZE)) {
      LOG_ERR("white_list", "failed to read mac");
      return false;
    }
    return w(std::move(addr));
  };
  item.item.mac.arg = const_cast<addr_f *>(&write_addr);
}

void set_decode_white_item_name(::WhiteItem &item, const name_f &write_name) {
  item.item.name.funcs.decode = [](pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
    if (arg == nullptr) {
      return false;
    }
    auto name     = Name{};
    const auto &w = *reinterpret_cast<name_f *>(*arg);
    // the 0x00 in the end?
    name.name.resize(stream->bytes_left + 1);
    if (!pb_read(stream, reinterpret_cast<pb_byte_t *>(name.name.data()), stream->bytes_left)) {
      LOG_ERR("white_list", "failed to read name");
      return false;
    }
    return w(std::move(name));
  };
  item.item.name.arg = const_cast<name_f *>(&write_name);
}

bool set_encode_white_item(const item_t &item, ::WhiteItem &pb_item) {
  if (std::holds_alternative<Name>(item)) {
    const auto &item_name = std::get<Name>(item);
    pb_item.which_item    = WhiteItem_name_tag;
    //  It can write as many or as few fields as it likes. For example,
    //  if you want to write out an array as repeated field, you should do it all in a single call.
    pb_item.item.name.funcs.encode = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
      const auto &name = *reinterpret_cast<const Name *>(*arg);
      if (!pb_encode_tag_for_field(stream, field)) {
        return false;
      }
      return pb_encode_string(stream, reinterpret_cast<const uint8_t *>(name.name.data()), name.name.size());
    };
    pb_item.item.name.arg = const_cast<Name *>(&item_name);
    return true;
  } else if (std::holds_alternative<Addr>(item)) {
    const auto &item_mac          = std::get<Addr>(item);
    pb_item.which_item            = WhiteItem_mac_tag;
    pb_item.item.mac.funcs.encode = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
      const auto &addr = *reinterpret_cast<const Addr *>(*arg);
      if (!pb_encode_tag_for_field(stream, field)) {
        return false;
      }
      return pb_encode_string(stream, addr.addr.data(), addr.addr.size());
    };
    pb_item.item.mac.arg = const_cast<Addr *>(&item_mac);
    return true;
  } else {
    return false;
  }
}

bool marshal_set_white_list(pb_ostream_t *ostream, ::WhiteListSet &set, list_t &list) {
  set.has_list                = true;
  set.list.items.funcs.encode = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
    const auto &list = *reinterpret_cast<const list_t *>(*arg);
    for (const auto &item : list) {
      // not very sure of creating a new item here
      ::WhiteItem pb_item;
      // Same as pb_encode_tag, except takes the parameters from a pb_field_iter_t structure.
      if (!set_encode_white_item(item, pb_item)) {
        LOG_ERR("white_list", "failed to set encode function.");
        return false;
      }
      if (!pb_encode_tag_for_field(stream, field)) {
        LOG_ERR("white_list", "failed to encode tag.");
        return false;
      }
      if (!pb_encode_submessage(stream, WhiteItem_fields, &pb_item)) {
        LOG_ERR("white_list", "failed to encode submessage");
        return false;
      }
    }
    return true;
  };
  set.list.items.arg = &list;
  // what's the difference between pb_encode and pb_encode_submessage?
  // The functions with names pb_encode_<datatype> are used when dealing with callback fields.
  // The typical reason for using callbacks is to have an array of unlimited size.
  // In that case, pb_encode will call your callback function,
  // which in turn will call pb_encode_<datatype> functions repeatedly to write out values.
  // https://jpa.kapsi.fi/nanopb/docs/reference.html#pb_encode
  return pb_encode(ostream, WhiteListSet_fields, &set);
}

etl::optional<list_t>
unmarshal_set_white_list(pb_istream_t *stream, ::WhiteListSet &set) {
  const auto TAG = "unmarshal_set_white_list";
  list_t result;
  // https://github.com/nanopb/nanopb/blob/master/tests/oneof_callback/oneof.proto
  auto white_list_decode = [](pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
    const auto TAG = "white_list_decode";
    // https://stackoverflow.com/questions/73529672/decoding-oneof-nanopb
    auto &result = *reinterpret_cast<list_t *>(*arg);
    LOG_INFO(TAG, "length: %zu", result.size());
    if (field->tag == WhiteList_items_tag) {
      LOG_INFO("parse_list", "one item");
      ::WhiteItem item = WhiteItem_init_zero;
      pb_wire_type_t wire_type;
      uint32_t tag;
      // don't mutate the original stream
      pb_istream_t s_copy = *stream;
      bool eof;
      pb_decode_tag(&s_copy, &wire_type, &tag, &eof);

      auto w_a = [&result](auto addr) {
        LOG_I("w_a", "here");
        std::cout << utils::toHex(addr.addr.data(), addr.addr.size()) << std::endl;
        result.emplace_back(item_t{addr});
        return true;
      };
      auto w_n = [&result](auto name) {
        LOG_I("w_n", "here");
        result.emplace_back(item_t{name});
        return true;
      };
      // https://github.com/nanopb/nanopb/blob/09234696e0ef821432a8541b950e8866f0c61f8c/examples/using_union_messages/decode.c#L24
      switch (tag) {
        case WhiteItem_name_tag:
          LOG_INFO("tag", "name %d", tag);
          item.which_item = tag;
          set_decode_white_item_name(item, w_n);
          break;
        case WhiteItem_mac_tag:
          LOG_INFO("tag", "mac %d", tag);
          item.which_item = tag;
          set_decode_white_item_addr(item, w_a);
          break;
        default:
          LOG_ERR("tag", "bad tag %d", tag);
          return false;
      }
      auto ok = pb_decode(stream, WhiteItem_fields, &item);
      if (!ok) {
        LOG_ERR("pl", "bad decode %s", stream->errmsg);
        return false;
      }
      return true;
    }
    return true;
  };
  set.list.items.funcs.decode = white_list_decode;
  set.list.items.arg          = &result;
  auto ok                     = pb_decode(stream, WhiteListSet_fields, &set);
  if (stream->errmsg != nullptr){
    LOG_ERR("white_list", "stream->errmsg %s", stream->errmsg);
  }
  if (!ok) {
    LOG_ERR("white_list", "failed to decode response");
    return etl::nullopt;
  }
  return result;
}
}
