project(font)
aux_source_directory(./ SOURCE)
include_directories(font PUBLIC
                        .
                        ${CMAKE_SOURCE_DIR}/lvgl-master/include
                        )
add_library(font STATIC ${SOURCE})