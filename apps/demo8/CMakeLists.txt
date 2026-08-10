cmake_minimum_required(VERSION 3.15)
project(main)

aux_source_directory(. SOURCES)
aux_source_directory(page/home SOURCES)
aux_source_directory(page/setting SOURCES)
aux_source_directory(page/setting/widgets SOURCES)

add_executable(main ${SOURCES})

target_include_directories(main PRIVATE
    ./
    page
    page/home
    page/setting
    page/setting/widgets
    res
    ${CMAKE_SOURCE_DIR}/lvgl-master
    ${CMAKE_SOURCE_DIR}/component/font
    ${CMAKE_SOURCE_DIR}/component/wifi
)

target_link_libraries(main PRIVATE
    lvgl
    lvgl_porting
    font
    wifi
)
