add_library(utils
SHARED
    buffer.cpp
)

target_include_directories(utils
PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_compile_features(utils
PUBLIC
    cxx_std_23
)

if(MSVC)
    target_compile_options(utils
    PRIVATE
        /W4
        /permissive
    )
else()
    target_compile_options(utils
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()