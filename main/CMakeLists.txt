idf_component_register(SRCS
        track_long_main.cpp
        Strip.cpp
        ScanCallback.cpp
        utils.cpp
        StripCallbacks.cpp
        INCLUDE_DIRS
        REQUIRES spi_flash esp_partition
        nanopb esp_timer arduino-libs
        arduino esp-nimble-cpp led_strip etl pthread)
