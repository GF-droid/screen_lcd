cmake_minimum_required(VERSION 3.15)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

project(main VERSION 1.0)

if(SIMULATOR_LINUX)
    message("Building for x86 Linux")
    # 定义 C 宏 SIMULATOR_LINUX, 供所有子目录源码 #ifdef 区分平台
    # (必须在 add_subdirectory 之前, 否则 apps/ 等兄弟目录看不到)
    add_definitions(-DSIMULATOR_LINUX)
    add_subdirectory(platform/x86linux)
else()
    message("Building for T113-S3")
    add_subdirectory(platform/t113)
endif()

add_subdirectory(component/osal)
add_subdirectory(component/font)
add_subdirectory(component/wifi)
add_subdirectory(lvgl-master)
add_subdirectory(apps/demo8)
# add_subdirectory(apps/demo6)
# add_subdirectory(apps/demo4)
# add_subdirectory(apps/demo3)
