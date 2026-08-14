cmake_minimum_required(VERSION 3.15)
project(demo9)

# 远程控制面板 agent: 只保留 网络(ws) + 控制(音量/亮度/闹钟) + 状态上报
aux_source_directory(. SOURCES)
aux_source_directory(net SOURCES)
aux_source_directory(ctrl SOURCES)
aux_source_directory(status SOURCES)

add_executable(demo9 ${SOURCES})

target_include_directories(demo9 PRIVATE
    ./
    ${CMAKE_SOURCE_DIR}/component
    ${CMAKE_SOURCE_DIR}/component/osal
)

if(SIMULATOR_LINUX)
    target_link_libraries(demo9 PRIVATE osal m)
else()
    target_link_libraries(demo9 PRIVATE osal m)
endif()
