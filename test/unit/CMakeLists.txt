set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/googletest ${CMAKE_CURRENT_BINARY_DIR}/googletest)

function(add_unit_test TEST_NAME)
    cmake_parse_arguments(TEST "" "" "SOURCES;LINK_LIBRARIES" ${ARGN})
    
    add_executable(${TEST_NAME} ${TEST_SOURCES})
    
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    
    target_link_libraries(${TEST_NAME} PRIVATE
        gtest_main
        glog::glog
        ${TEST_LINK_LIBRARIES}
    )
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 60
        LABELS "unit"
    )
endfunction()

add_unit_test(fan_calculator_test
    SOURCES fan_calculator_test.cpp
    LINK_LIBRARIES fan_calculator_core
)

add_unit_test(storage_test
    SOURCES storage_test.cpp
    LINK_LIBRARIES storage
)
