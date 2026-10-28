add_library(dsp_modulation
SHARED
    blocker.cpp
    demodulate.cpp
)

target_compile_features(dsp_modulation
PUBLIC
    cxx_std_23
)

if(MSVC)
    target_compile_options(dsp_modulation
    PRIVATE
        /W4
        /permissive
    )
else()
    target_compile_options(dsp_modulation
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()