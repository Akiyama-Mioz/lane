//
// Created by Kurosu Chan on 2023/11/7.
//

#ifndef TRACK_SHORT_HR_DATA_WITH_NAME_H
#define TRACK_SHORT_HR_DATA_WITH_NAME_H

namespace HrLoRa {
struct named_hr_data {
  static constexpr uint8_t magic = 0x60;
  struct t {
    using module = named_hr_data;
    uint8_t key  = 0;
    uint8_t hr   = 0;
    std::string name{};
  };
  static size_t size_needed(const t &data) {
    // key + hr + magic
    return sizeof(magic) + sizeof(t::key) + sizeof(t::hr) + data.name.size() + 1;
  }
  static size_t marshal(const t &data, uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < size_needed(data)) {
      return 0;
    }
    size_t offset    = 0;
    buffer[offset++] = magic;
    buffer[offset++] = data.key;
    buffer[offset++] = data.hr;
    std::copy(data.name.begin(), data.name.end(), buffer + 3);
    offset += data.name.size();
    buffer[offset++] = 0;
    return offset;
  }
  static etl::optional<t> unmarshal(const uint8_t *buffer, size_t size) {
    if (size < size_needed(t{})) {
      return etl::nullopt;
    }

    t data;
    if (buffer[0] != magic) {
      return etl::nullopt;
    }

    data.key  = buffer[1];
    data.hr   = buffer[2];
    data.name = std::string(reinterpret_cast<const char *>(buffer + 3));
    return data;
  }
};
}

#endif // TRACK_SHORT_HR_DATA_WITH_NAME_H
