cmake_minimum_required(VERSION 3.15)
project(network C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(network main.c transport.h name_resolution.h http_client.h http_server.h)
target_compile_options(network PRIVATE -W -fverbose-asm -save-temps)
