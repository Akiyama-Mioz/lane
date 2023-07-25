idf_component_register(SRCS
        lane_main.cpp
        Lane.cpp
        ScanCallback.cpp
        utils.cpp
        LaneCallbacks.cpp
        INCLUDE_DIRS
        REQUIRES spi_flash esp_partition
        nanopb esp_timer arduino-libs
        arduino esp-nimble-cpp led_strip etl pthread)
