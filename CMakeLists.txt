cmake_minimum_required(VERSION 3.22.1)
project(cewserver C)

set(CMAKE_C_STANDARD 11)

add_executable(cewserver src/main.c)

# strip in release
set_target_properties(cewserver PROPERTIES LINK_FLAGS_RELEASE -s)

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