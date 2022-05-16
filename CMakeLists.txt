cmake_minimum_required(VERSION 3.21)
project(server C)

set(CMAKE_C_STANDARD 99)

include_directories(includes)
add_executable(server main.c shmalloc.c dynString.c dynHashmap.c dynArray.c network.c command.c storage.c lock.c systemExec.c httpInterface.c)
target_link_libraries(server m)