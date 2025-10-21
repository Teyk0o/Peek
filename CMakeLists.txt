cmake_minimum_required(VERSION 3.27)
project(Peek C)

set(CMAKE_C_STANDARD 11)

add_executable(Peek main.c)

target_link_libraries(Peek ws2_32 iphlpapi)