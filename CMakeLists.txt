cmake_minimum_required(VERSION 3.10)
project(tziakcha_record_miner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

set(BUILD_TESTING OFF)

set(BUILD_TESTING OFF CACHE BOOL "Disable glog tests" FORCE)
add_subdirectory(third_party/glog)
add_subdirectory(third_party/cxxopts)
set(BUILD_TESTING ON)

add_subdirectory(src)
add_subdirectory(test)
