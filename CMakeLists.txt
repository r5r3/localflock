cmake_minimum_required(VERSION 3.0)
project(localflock VERSION 0.1)

# our target is a single library file
file(GLOB sources src/*.cpp)
add_library(localflock SHARED ${sources})
set_property(TARGET localflock PROPERTY CXX_STANDARD 17)

# we link against dl and pthreads
target_link_libraries(localflock -ldl)
target_link_libraries(localflock -lpthread)
target_link_libraries(localflock -lgcrypt)
target_link_libraries(localflock -static-libgcc -static-libstdc++)