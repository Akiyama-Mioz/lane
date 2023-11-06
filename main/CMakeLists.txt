idf_component_register(
        SRCS
        lane_main.cpp
        Lane.cpp
        LaneCallback.cpp
        ScanCallback.cpp
        whitelist.cpp
        whitelist_esp.cpp
        utils.cpp

        INCLUDE_DIRS
        inc

        REQUIRES
        spi_flash
        nanopb
        esp_timer
        NeoPixel
        RadioLib
        arduino
        esp-nimble-cpp
        etl
        pthread)

# https://github.com/sheredom/utest.h/issues/19
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++2a -Wformat=0)
