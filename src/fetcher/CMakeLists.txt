add_library(fetcher
    history_fetcher.cpp
    session_fetcher.cpp
    record_fetcher.cpp
    ../../third_party/cpp-base64/base64.cpp
)

target_compile_definitions(fetcher PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_include_directories(fetcher PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cpp-httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cpp-base64
)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

target_link_libraries(fetcher PUBLIC
    glog::glog
    fetcher_config
    storage
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    utils
)

add_executable(fetcher_cli 
    fetcher_cli.cpp
)

target_include_directories(fetcher_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(fetcher_cli PRIVATE
    fetcher
    cxxopts
    glog::glog
)
