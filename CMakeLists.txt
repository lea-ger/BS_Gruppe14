cmake_minimum_required(VERSION 3.21)
project(server C)

set(CMAKE_C_STANDARD 99)

include_directories(includes)
add_executable(server main.c dynString.c dynArray.c network.c command.c storage.c lock.c newsletter.c systemExec.c httpInterface.c)
