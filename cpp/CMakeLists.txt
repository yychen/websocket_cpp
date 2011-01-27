cmake_minimum_required (VERSION 2.6)
project (websocket_cpp)

set (websocket_cpp_VERSION_MAJOR 0)
set (websocket_cpp_VERSION_MINOR 1)

configure_file (
    "${PROJECT_SOURCE_DIR}/Config.h.in"
    "${PROJECT_SOURCE_DIR}/Config.h"
)

include_directories("${PROJECT_BINARY_DIR}")

add_executable(main SocketServer.cpp main.cpp)
target_link_libraries(main ssl pthread)
