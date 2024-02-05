cmake_minimum_required(VERSION 3.22.1)
project(cewserver LANGUAGES C VERSION 0.1)

set(CMAKE_C_STANDARD 11)

add_executable(cewserver
        src/main.c
        src/settings.c
        include/settings.h
        external/sds/sds.c
        external/sds/sds.h
        external/sds/sdsalloc.h
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
        -Wpedantic
        -Wformat
        -Wformat-security
        -Werror=format-security
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

# Just for testing
target_link_libraries( cewserver ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries( cewserver rt )
