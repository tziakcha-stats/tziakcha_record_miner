add_executable(verify_fan_calculator
    verify_fan_calculator.cpp
)

target_link_libraries(verify_fan_calculator
    analyzer
    fan_calculator_core
    storage
    utils
    fetcher_config
    glog::glog
    nlohmann_json::nlohmann_json
)

target_include_directories(verify_fan_calculator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

set_target_properties(verify_fan_calculator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/scripts"
)

message(STATUS "Configured verification tool: verify_fan_calculator")
