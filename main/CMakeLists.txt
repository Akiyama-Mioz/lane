idf_component_register(
        SRCS
        lane_main.cpp
        Lane.cpp
        LaneCallback.cpp
        ScanCallback.cpp
        whitelist.cpp
        whitelist_esp.cpp
        utils.cpp
        EspHal.cpp

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

target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++2a -Wformat=0)
