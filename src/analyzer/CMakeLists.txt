add_library(analyzer
    record_parser.cpp
    game_state.cpp
    action.cpp
    win_analyzer.cpp
    simulator.cpp
    core.cpp
)

target_include_directories(analyzer PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(analyzer PUBLIC
    utils
    fan_calculator_core
    glog::glog
)
add_executable(analyzer_cli analyzer_cli.cpp)

target_link_libraries(analyzer_cli PRIVATE
    analyzer
    utils
    glog::glog
    cxxopts::cxxopts
)