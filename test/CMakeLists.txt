cmake_minimum_required(VERSION 3.22.1)
project(cewserver_test LANGUAGES C)

set(CMAKE_C_STANDARD 11)

add_executable(cewserver_test
        test.c
)

target_include_directories(cewserver_test PRIVATE
        ../include
        ../external/libini
        ../external/libev
        ../external/sds
)

# add library search paths
target_link_directories(cewserver_test PRIVATE
        ../external/libini/build/
        ../external/libev/.libs
)

# What libraries to use
target_link_libraries(cewserver_test
        libini.a
        libev.a
)

# Just for testing
target_link_libraries(cewserver_test ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(cewserver_test rt cmocka)
