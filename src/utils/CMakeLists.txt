add_library(utils
    script_decoder.cpp
    tile.cpp
    ../../third_party/cpp-base64/base64.cpp
)

target_include_directories(utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cpp-base64
)

find_package(ZLIB REQUIRED)

target_link_libraries(utils PUBLIC
    glog::glog
    ZLIB::ZLIB
)
