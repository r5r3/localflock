cmake_minimum_required(VERSION 3.0)
project(localflock VERSION 0.1)

# we need a compiler with c++17 support
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1")
        message(FATAL_ERROR "Insufficient gcc version")
    endif()
endif()

# our target is a single library file
file(GLOB sources src/*.cpp)
add_library(localflock SHARED ${sources})
set_property(TARGET localflock PROPERTY CXX_STANDARD 17)

# we link against dl and pthreads
target_link_libraries(localflock -ldl)
target_link_libraries(localflock -lpthread)
target_link_libraries(localflock -lgcrypt)
target_link_libraries(localflock -static-libgcc -static-libstdc++)