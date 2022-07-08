file(GLOB NANOPB_SRC nanopb/pb_common.c nanopb/pb_decode.c nanopb/pb_encode.c)
file(GLOB NANOPB_CPP_SRC nanopb_cpp/nanopb_cpp.cpp)

set(NANOPB_SRC_FILES
    $(NANOPB_SRC)
    $(NANOPB_CPP_SRC)
    )

set(NANOPB_INCLUDE_DIR
        nanopb)

set(NANOPB_CPP_INCLUDE_DIR
        nanopb_cpp)

idf_component_register(
        SRC ${NANOPB_SRC_FILES}
        INCLUDE_DIRS ${NANOPB_INCLUDE_DIR} ${NANOPB_CPP_INCLUDE_DIR})
