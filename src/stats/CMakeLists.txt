add_library(stats
    intercept_stats.cpp
)

target_include_directories(stats PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(stats PUBLIC
    utils
    glog::glog
)
