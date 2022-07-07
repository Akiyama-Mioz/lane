file(GLOB PubSubClient_SRC pubsubclient/src/*.cpp)

set(LIB_INCLUDE_DIRS
        pubsubclient/src/)

set(LIB_SRCS
        ${PubSubClient_SRC})

idf_component_register(SRCS ${LIB_SRCS}
        INCLUDE_DIRS ${LIB_INCLUDE_DIRS}
        REQUIRES arduino)