add_library(fetcher_config
    fetcher_config.cpp
)

target_include_directories(fetcher_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/json/include
)
