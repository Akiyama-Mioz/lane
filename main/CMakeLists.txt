idf_component_register(SRCS
        lane_main.cpp
        Lane.cpp
        LaneCallback.cpp
        ScanCallback.cpp
        utils.cpp
        INCLUDE_DIRS
        REQUIRES spi_flash
        nanopb esp_timer NeoPixel
        arduino esp-nimble-cpp etl pthread)
