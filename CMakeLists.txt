cmake_minimum_required(VERSION 3.25)
project(cLox C)

set(CMAKE_C_STANDARD 11)

add_executable(cLox main.c common.h chunk.h chunk.c memory.c memory.h debug.c debug.h value.c value.h vm.c vm.h compiler.c compiler.h scanner.c scanner.h object.c object.h)

