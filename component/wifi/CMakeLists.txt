project(wifi)

aux_source_directory(./ SOURCE)
aux_source_directory(./utils SOURCE)

include_directories(wifi PUBLIC
    .
    ./utils
    ./common
    ${CMAKE_SOURCE_DIR}/component/osal
)

add_library(wifi STATIC ${SOURCE})

# wpa_ctrl_* 函数在预编译库里: x86 Linux 用 .a, T113 用 .so
if(SIMULATOR_LINUX)
    target_link_libraries(wifi PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/libwpa_client.a
        osal
    )
else()
    target_link_libraries(wifi PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/libwpa_client.so
        osal
    )
endif()
