file(GLOB PubSubClient_SRC pubsubclient/src/*.cpp)
file(GLOB NeoPixel_SRC Adafruit_NeoPixel/Adafruit_NeoPixel.cpp Adafruit_NeoPixel/esp.c)

set(LIB_INCLUDE_DIRS
        pubsubclient/src/
        Adafruit_NeoPixel/
        )

set(LIB_SRCS
        ${PubSubClient_SRC}
        ${NeoPixel_SRC})

idf_component_register(SRCS ${LIB_SRCS}
        INCLUDE_DIRS ${LIB_INCLUDE_DIRS}
        REQUIRES arduino)