cmake_minimum_required(VERSION 3.10)
project(simplehttp)
find_package(OpenSSL REQUIRED)
add_library(simplehttp STATIC 
    src/server.cpp
    src/threadpool.cpp
)
target_include_directories(simplehttp PUBLIC include)
target_include_directories(simplehttp PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(simplehttp PRIVATE OpenSSL::SSL OpenSSL::Crypto)