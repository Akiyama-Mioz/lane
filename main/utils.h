//
// Created by Kurosu Chan on 2022/8/4.
//
// necessary for using fmt library
#define FMT_HEADER_ONLY

#ifndef HELLO_WORLD_UTILS_H
#define HELLO_WORLD_UTILS_H

#include "fmt/core.h"

std::string to_hex(const std::basic_string<char> &s);
std::string to_hex(const char *s, size_t len);

#endif //HELLO_WORLD_UTILS_H
