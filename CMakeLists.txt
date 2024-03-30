cmake_minimum_required(VERSION 3.27)
project(LightSim C)

set(CMAKE_C_STANDARD 99)

add_executable(LightSim WIN32 main.c
        render.c
        render.h)

target_compile_options(LightSim PRIVATE -Wall -Wextra -Wpedantic -march=native)
target_compile_definitions(LightSim PRIVATE NOMINMAX)
