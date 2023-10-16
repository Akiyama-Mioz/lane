//
// Created by Kurosu Chan on 2023/10/10.
//

#include "whitelist.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include <etl/optional.h>

#ifdef ESP32
#define LOG_ERR(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#elif defined(SIMPLE_LOG)
#define LOG_ERR(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
#else
#define LOG_ERR(tag, fmt, ...) // Define an empty macro if none of the conditions are met
#endif

namespace white_list {
    // decode one
    void set_decode_white_item(pb_istream_t *stream, ::WhiteItem &item, item_t &to_be_write) {
        item.item.mac.funcs.decode = [](pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
            if (arg == nullptr) {
                return false;
            }
            auto addr = Addr{};
            // Use pb_read instead, length of the string is available in stream->bytes_left.
            // https://jpa.kapsi.fi/nanopb/docs/concepts.html#decoding-callbacks
            // https://github.com/nanopb/nanopb/blob/09234696e0ef821432a8541b950e8866f0c61f8c/tests/callbacks/decode_callbacks.c#L10

            auto &item = *reinterpret_cast<item_t *>(*arg);
            if (!pb_read(stream, addr.addr.data(), BLE_MAC_ADDR_SIZE)) {
                LOG_ERR("white_list", "failed to read mac");
                return false;
            }
            auto new_item = item_t(addr);
            // https://en.cppreference.com/w/cpp/algorithm/swap
            std::swap(item, new_item);
            return true;
        };
        item.item.mac.arg = &to_be_write;
        item.item.name.funcs.decode = [](pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
            if (arg == nullptr) {
                return false;
            }
            auto name = Name{};
            auto &item = *reinterpret_cast<item_t *>(*arg);
            // the 0x00 in the end?
            name.name.resize(stream->bytes_left + 1);
            if (!pb_read(stream, reinterpret_cast<pb_byte_t *>(name.name.data()), stream->bytes_left)) {
                LOG_ERR("white_list", "failed to read name");
                return false;
            }
            auto new_item = item_t{name};
            std::swap(item, new_item);
            return true;
        };
        item.item.name.arg = &to_be_write;
    }

    bool set_encode_white_item(pb_ostream_t *ostream, const item_t &item, ::WhiteItem &pb_item) {
        if (holds_alternative<Name>(item)) {
            const auto &item_name = get<Name>(item);
            pb_item.which_item = WhiteItem_name_tag;
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
        } else if (holds_alternative<Addr>(item)) {
            const auto &item_mac = get<Addr>(item);
            pb_item.which_item = WhiteItem_mac_tag;
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

    etl::optional<list_t>
    parse_set_white_list(pb_istream_t *stream, ::WhiteListSet &set) {
        list_t result;
        auto white_list_decode = [](pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
            if (arg == nullptr) {
                return false;
            }
            // https://stackoverflow.com/questions/73529672/decoding-oneof-nanopb
            auto &result = *reinterpret_cast<std::vector<item_t> *>(*arg);
            // I'm not use if this the correct way
            // TODO: find the documentation or source code
            ::WhiteItem &pb_item = *static_cast<::WhiteItem *>(field->message);
            auto to_be_swap = item_t{};
            set_decode_white_item(stream, pb_item, to_be_swap);
            return true;
        };
        set.list.items.funcs.decode = white_list_decode;
        set.list.items.arg = &result;
        if (!pb_decode(stream, WhiteListSet_fields, &set)) {
            LOG_ERR("white_list", "failed to decode response");
            return etl::nullopt;
        }
        return result;
    }
}
