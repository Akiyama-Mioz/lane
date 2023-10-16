cmake_minimum_required(VERSION 3.20)

project(nanopb_test)
add_subdirectory(../components/etl/etl)
add_subdirectory(../components/nanopb/nanopb)

set(WHITE_LIST_SRC ../main/whitelist.cpp ../main/whitelist.h)
add_executable(test main.cpp ${WHITE_LIST_SRC})
