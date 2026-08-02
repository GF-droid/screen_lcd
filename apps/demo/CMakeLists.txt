cmake_minimum_required(VERSION 3.15)
project(main)

aux_source_directory(. SOURCES)

add_executable(main ${SOURCES})

target_include_directories(main PRIVATE
    ./
    ${CMAKE_SOURCE_DIR}/lvgl-master
    ${CMAKE_SOURCE_DIR}/apps/demo
)

target_link_libraries(main PRIVATE
    lvgl
    lvgl_porting
)
