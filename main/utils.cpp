//
// Created by Kurosu Chan on 2022/8/4.
//
#include "utils.h"


std::string to_hex(const std::basic_string<char> &s) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (auto c: s) {
    ss << std::hex << std::setw(2) << static_cast<int>(c);
  }
  return ss.str();
}

std::string to_hex(const uint8_t *v, const size_t s) {
  std::stringstream ss;

  ss << std::hex << std::setfill('0');

  for (int i = 0; i < s; i++) {
    ss << std::hex << std::setw(2) << static_cast<int>(v[i]);
  }

  return ss.str();
}
