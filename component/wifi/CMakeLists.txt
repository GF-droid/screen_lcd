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

# wpa_ctrl_* 函数在预编译库里: x86 用 .a, T113 换 .so
# 用绝对路径避免 link_directories 的作用域问题 (子目录设置对父目录不生效)
target_link_libraries(wifi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/libwpa_client.a
    osal
)
