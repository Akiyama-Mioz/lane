//
// Created by Kurosu Chan on 2023/10/10.
//

#include "whitelist.h"
#include "pb_decode.h"
#include <etl/optional.h>

namespace white_list {
    etl::optional<item_t>
    parse_white_item(pb_istream_t *stream, ::WhiteItem &item) {
        std::string name{};
        std::array<uint8_t, BLE_MAC_ADDR_SIZE> addr{};
        item.item.mac.funcs.decode = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
            if (arg == nullptr) {
                return false;
            }
            // Use pb_read instead, length of the string is available in stream->bytes_left.
            // https://github.com/nanopb/nanopb/blob/09234696e0ef821432a8541b950e8866f0c61f8c/tests/callbacks/decode_callbacks.c#L10
            auto &addr = *reinterpret_cast<std::array<uint8_t, BLE_MAC_ADDR_SIZE> *>(*arg);
            if (!pb_read(stream, addr.data(), BLE_MAC_ADDR_SIZE)) {
#ifdef ESP32
                ESP_LOGE("white_list", "failed to read mac");
#elif defined(SIMPLE_LOG)
                LOG_E("white_list", "failed to read mac");
#endif
                return false;
            }
            return true;
        };
        item.item.mac.arg = &addr;
        item.item.name.funcs.decode = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
            if (arg == nullptr) {
                return false;
            }
            auto &name = *reinterpret_cast<std::string *>(*arg);
            // the 0x00 in the end?
            name.resize(stream->bytes_left + 1);
            if (!pb_read(stream, reinterpret_cast<pb_byte_t *>(name.data()), stream->bytes_left)) {
#ifdef ESP32
                ESP_LOGE("white_list", "failed to read name");
#endif
                return false;
            }
            return true;
        };
        item.item.name.arg = &name;
        if (!pb_decode(stream, WhiteItem_fields, &item)) {
#ifdef ESP32
            ESP_LOGE("white_list", "failed to decode item");
#endif
            return etl::nullopt;
        }
        switch (item.which_item) {
            case WhiteItem_name_tag: {
                auto n = Name{name};
                auto i = item_t(n);
                return i;
            }
            case WhiteItem_mac_tag: {
                auto a = Addr{addr};
                auto i = item_t(a);
                return i;
            }
            default:
                return etl::nullopt;
        }
    }

    etl::optional<response_t>
    parse_white_list_response(pb_istream_t *stream, ::WhiteListResponse &response) {
        list_t result;
        auto white_list_decode = [](pb_istream_t *stream, const pb_field_t *field, void **arg) {
            if (arg == nullptr) {
                return false;
            }
            // https://stackoverflow.com/questions/73529672/decoding-oneof-nanopb
            auto &result = *reinterpret_cast<std::vector<item_t> *>(*arg);
            ::WhiteItem &item = *(::WhiteItem *) field->message;
            auto item_opt = parse_white_item(stream, item);
            if (!item_opt.has_value()) {
                return false;
            } else {
                result.push_back(item_opt.value());
            }
            return true;
        };
        response.response.list.items.funcs.decode = white_list_decode;
        response.response.list.items.arg = &result;
        if (!pb_decode(stream, WhiteListResponse_fields, &response)) {
#ifdef ESP32
            ESP_LOGE("white_list", "failed to decode response");
#endif
            return etl::nullopt;
        }
        switch (response.which_response) {
            case WhiteListResponse_list_tag: {
                return response_t(result);
            }
            case WhiteListResponse_code_tag: {
                return response_t(response.response.code);
            }
            default:
                return etl::nullopt;
        }
    }
}
