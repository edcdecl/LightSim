cmake_minimum_required(VERSION 3.27)
project(LightSim CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(LightSim WIN32 main.cpp render.cpp)

target_compile_options(LightSim PRIVATE -Wall -Wextra -Wpedantic -march=native
        -ffinite-math-only
        -fno-math-errno
        -fno-trapping-math
        -fno-signed-zeros
        -fopenmp
)
target_compile_definitions(LightSim PRIVATE NOMINMAX)
target_include_directories(LightSim PRIVATE SYSTEM ${CMAKE_SOURCE_DIR}/libinc)
target_link_libraries(LightSim PRIVATE libomp)
