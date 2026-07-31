cmake_minimum_required(VERSION 3.12.4)
project(lvgl_demo LANGUAGES C)

# 设置 C 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加 LVGL 库 (lv_conf.h 在当前目录，会被自动找到)
add_subdirectory(lvgl-master)

# 查找 SDL2
find_package(SDL2 REQUIRED)

# 创建可执行文件
add_executable(demo main.c)

# 链接库
target_link_libraries(demo PRIVATE lvgl SDL2::SDL2)

# 添加当前目录到 include path (让 lv_conf.h 能被找到)
# LVGL 的 CMake 会自动在上级目录查找 lv_conf.h
target_include_directories(demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
