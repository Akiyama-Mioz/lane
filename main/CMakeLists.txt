idf_component_register(SRCS
        lane_main.cpp
        Lane.cpp
        LaneCallback.cpp
        ScanCallback.cpp
        whitelist.cpp
        utils.cpp
        INCLUDE_DIRS
        REQUIRES spi_flash
        nanopb esp_timer NeoPixel
        arduino esp-nimble-cpp etl pthread)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17 -Wformat=0)
