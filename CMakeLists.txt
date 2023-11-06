# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

# personal settings
# set(IDF_MAINTAINER crosstyan)
# set(CMAKE_C_FLAGS "-Wno-attributes -Wno-format -Wno-format-extra-args -Wno-format-overflow -Wno-format-security -Wno-format-signedness -Wno-maybe-uninitialized")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_definitions(ARDUINO_ARCH_ESP32=1)

# https://cmake.org/cmake/help/latest/envvar/CFLAGS.html
project(track_short)

