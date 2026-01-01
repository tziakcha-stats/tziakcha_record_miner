set(GB_MAHJONG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/GB-Mahjong)
set(GB_MAHJONG_SOURCES
    ${GB_MAHJONG_DIR}/console/console.cpp
    ${GB_MAHJONG_DIR}/console/print.cpp
    ${GB_MAHJONG_DIR}/mahjong/fan.cpp
    ${GB_MAHJONG_DIR}/mahjong/handtiles.cpp
    ${GB_MAHJONG_DIR}/mahjong/pack.cpp
    ${GB_MAHJONG_DIR}/mahjong/tile.cpp
)

add_library(fan_calculator_core STATIC
    fan_calculator_core.cpp
    ${GB_MAHJONG_SOURCES}
)

target_include_directories(fan_calculator_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${GB_MAHJONG_DIR}/console
    ${GB_MAHJONG_DIR}/mahjong
)

target_link_libraries(fan_calculator_core PUBLIC
    glog::glog
)

add_executable(fan_calculator 
    fan_calculator.cpp
)

target_include_directories(fan_calculator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(fan_calculator PRIVATE
    fan_calculator_core
    cxxopts
)
