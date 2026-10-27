add_library(dsp_utils
SHARED
    buffer.cpp
    sampler.cpp
)

target_compile_features(dsp_utils
PUBLIC
    cxx_std_23
)

if(MSVC)
    target_compile_options(dsp_utils
    PRIVATE
        /W4
        /permissive
    )
else()
    target_compile_options(dsp_utils
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()