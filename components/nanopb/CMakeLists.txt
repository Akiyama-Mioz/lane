#file(GLOB NANOPB_SRC )
#file(GLOB NANOPB_CPP_SRC )
#file(GLOB GEN_PROTOBUF_SRC )
#
#set(NANOPB_SRC_FILES
#    $(NANOPB_SRC)
#    $(NANOPB_CPP_SRC)
#    )

set(NANOPB_INCLUDE_DIR
        nanopb)

set(NANOPB_CPP_INCLUDE_DIR
        nanopb_cpp)

set(GEN_PROTOBUF_INCLUDE_DIR
        protobuf)

idf_component_register(
        SRCS nanopb/pb_common.c nanopb/pb_decode.c nanopb/pb_encode.c nanopb_cpp/nanopb_cpp.cpp protobuf/ble.pb.c
        protobuf/track_config.pb.c
        protobuf/track_option.pb.c
        INCLUDE_DIRS ${NANOPB_INCLUDE_DIR} ${NANOPB_CPP_INCLUDE_DIR} ${GEN_PROTOBUF_INCLUDE_DIR}
)
