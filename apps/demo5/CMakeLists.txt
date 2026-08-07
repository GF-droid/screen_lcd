cmake_minimum_required(VERSION 3.15)
project(main)

aux_source_directory(. SOURCES)
aux_source_directory(page SOURCES)

add_executable(main ${SOURCES})

target_include_directories(main PRIVATE
    ./
    page
    res
    ${CMAKE_SOURCE_DIR}/lvgl-master
    ${CMAKE_SOURCE_DIR}/apps/demo2/page
    ${CMAKE_SOURCE_DIR}/apps/demo2
    ${CMAKE_SOURCE_DIR}/apps/demo2/res/font
    ${CMAKE_SOURCE_DIR}/component/font
    ${CMAKE_SOURCE_DIR}/component/wifi
)

target_link_libraries(main PRIVATE
    lvgl
    lvgl_porting
    font
    wifi
)
