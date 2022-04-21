cmake_minimum_required(VERSION 3.7.2)
project(PrakBS22 C)

set(CMAKE_C_STANDARD 99)

add_executable(PrakBS22 main.c main.h keyValStore.c keyValStore.h sub.c sub.h)
