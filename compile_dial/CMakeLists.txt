cmake_minimum_required(VERSION 3.18)
project(compile_dial)

set(CMAKE_CXX_STANDARD 17)
include_directories(
        ./
)

add_executable(
        compile_dial
        compile_iwf.c
        compile_iwf.h
)
