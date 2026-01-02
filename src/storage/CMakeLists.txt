add_library(storage
    filesystem_storage.cpp
)

target_include_directories(storage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/json/include
)

target_link_libraries(storage PUBLIC
    glog::glog
)
