//
// Created by Kurosu Chan on 2022/8/4.
//
#include "utils.h"


std::string to_hex(const std::basic_string<char> &s) {
  std::string res;
  for (auto c: s) {
    res += fmt::format("{:02x}", c);
  }
  return res;
}

std::string to_hex(const char *s, size_t len) {
  std::string res;
  for (size_t i = 0; i < len; i++) {
    res += fmt::format("{:02x}", s[i]);
  }
  return res;
}
