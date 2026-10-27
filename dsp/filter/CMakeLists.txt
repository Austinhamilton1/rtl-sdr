add_library(dsp_filter
SHARED
    biquad.cpp
    tuner.cpp
)

target_compile_features(dsp_filter
PUBLIC
    cxx_std_23
)

if(MSVC)
    target_compile_options(dsp_filter
    PRIVATE
        /W4
        /permissive
    )
else()
    target_compile_options(dsp_filter
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()