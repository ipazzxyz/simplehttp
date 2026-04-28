cmake_minimum_required(VERSION 3.10)
project(testing2)
find_package(OpenSSL REQUIRED)
add_executable(server
    src/main.cpp
    src/server.cpp
    src/threadpool.cpp)
target_include_directories(server PRIVATE src)
target_include_directories(server PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(server PRIVATE OpenSSL::SSL OpenSSL::Crypto)