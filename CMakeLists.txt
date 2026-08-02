cmake_minimum_required(VERSION 3.15)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

project(main VERSION 1.0)

if(SIMULATOR_LINUX)
    message("Building for x86 Linux")
    add_subdirectory(platform/x86linux)
else()
    message("Building for T113-S3")
    add_subdirectory(platform/t113)
endif()

add_subdirectory(component/osal)
add_subdirectory(lvgl-master)
add_subdirectory(apps/demo)
