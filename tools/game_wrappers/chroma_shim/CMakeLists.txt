cmake_minimum_required(VERSION 3.16)
project(openrgb_chroma_shim LANGUAGES CXX)

add_library(chroma_shim SHARED
    chroma_shim.cpp
    ../common/gw_udp_emit.cpp
)
target_link_libraries(chroma_shim PRIVATE ws2_32)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(chroma_shim PROPERTIES
        OUTPUT_NAME RazerChromaSDK64
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/chroma_shim.def"
    )
else()
    set_target_properties(chroma_shim PROPERTIES
        OUTPUT_NAME RazerChromaSDK
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/chroma_shim.def"
    )
endif()

set_target_properties(chroma_shim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
