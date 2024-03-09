cmake_minimum_required(VERSION 3.22.1)
project(cewserver LANGUAGES C VERSION 0.1)

set(CMAKE_C_STANDARD 11)

#------------------------------------
# Cmocka unittest

add_executable(cewserver_test
        ./test/cew_test.c
        src/cew_exit.c
        external/sds/sds.c
)

target_include_directories(cewserver_test PRIVATE
        include
        external/libini
        external/libev
        external/sds
        /usr/include/x86_64-linux-gnu/curl/
)

# add library search paths
target_link_directories(cewserver_test PRIVATE
        external/libini/build/
        external/libev/.libs
)

# What libraries to use
target_link_libraries(cewserver_test
        libini.a
        libev.a
)

# Just for testing
target_link_libraries(cewserver_test ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(cewserver_test rt cmocka curl)


#------------------------------------------------

add_executable(cewserver
        src/cew_main.c
        src/cew_settings.c
        external/sds/sds.c
        src/cew_logger.c
        src/cew_client.c
        src/cew_socket.c
        src/cew_worker.c
        src/cew_exit.c
        src/cew_http.c
)

target_include_directories(cewserver PRIVATE
        include
        external/libini
        external/libev
        external/sds
)

set_target_properties(cewserver PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

# strip in release
set_target_properties(cewserver PROPERTIES LINK_FLAGS_RELEASE -s)

# disable RPATH
# https://wiki.debian.org/RpathIssue
set_target_properties(cewserver PROPERTIES SKIP_BUILD_RPATH TRUE)

# https://wiki.debian.org/Hardening
# https://www.redhat.com/en/blog/hardening-elf-binaries-using-relocation-read-only-relro
target_compile_options(cewserver PRIVATE
        -Wall
        -Wextra
#        -Werror
        -Wpedantic
        -Wformat
        -Wformat-security
        -Werror=format-security
        -Wno-unused-parameter       #for pthreads
        -Wno-error=strict-aliasing  # for ev
        $<$<CONFIG:DEBUG>:-g3>
        $<$<CONFIG:DEBUG>:-Og>
        $<$<CONFIG:DEBUG>:-ggdb>
        $<$<CONFIG:DEBUG>:-gdwarf>
        $<$<CONFIG:RELEASE>:-O3>
        $<$<CONFIG:RELEASE>:-fPIE>
        $<$<CONFIG:RELEASE>:-pie>
        $<$<CONFIG:RELEASE>:-fstack-protector-all>
        $<$<CONFIG:RELEASE>:-D_FORTIFY_SOURCE=2>
)

target_link_options(cewserver PRIVATE
        $<$<CONFIG:RELEASE>:-z,relro>
        $<$<CONFIG:RELEASE>:-z,now>
)

add_compile_definitions(
        $<$<CONFIG:DEBUG>:DEBUG>
)

#target_include_directories(cewserver PRIVATE external/libini/) // does work, but clion can't do check

# add library search paths
target_link_directories(cewserver PRIVATE
        external/libini/build/
        external/libev/.libs
)

# What libraries to use
target_link_libraries(cewserver
        libini.a
        libev.a
)

# pthread
target_link_libraries( cewserver ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries( cewserver rt )