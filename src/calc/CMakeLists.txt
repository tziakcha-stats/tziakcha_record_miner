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
    fan_calculator.cpp
    ${GB_MAHJONG_SOURCES}
)
set_target_properties(fan_calculator_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(fan_calculator_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${GB_MAHJONG_DIR}/console
    ${GB_MAHJONG_DIR}/mahjong
)

target_link_libraries(fan_calculator_core PUBLIC
    glog::glog
)

add_executable(calc_cli 
    calc_cli.cpp
)

target_include_directories(calc_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(calc_cli PRIVATE
    fan_calculator_core
    cxxopts
)
