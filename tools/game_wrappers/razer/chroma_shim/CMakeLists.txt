cmake_minimum_required(VERSION 3.16)
project(openrgb_chroma_shim LANGUAGES CXX)

# Colore (Corale.Colore) loads RzChromaSDK64.dll / RzChromaSDK.dll.
# Native Razer SDK games load RazerChromaSDK64.dll — copy/rename at install time if needed.
add_library(chroma_shim SHARED
    chroma_shim.cpp
    ../../common/gw_udp_emit.cpp
    ../../common/gw_capture_log.cpp
)
target_link_libraries(chroma_shim PRIVATE ws2_32 ole32)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_shim_out RzChromaSDK64)
else()
    set(_shim_out RzChromaSDK)
endif()

set_target_properties(chroma_shim PROPERTIES
    OUTPUT_NAME "${_shim_out}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    set_target_properties(chroma_shim PROPERTIES
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/chroma_shim.def"
    )
endif()
