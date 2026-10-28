add_library(backends
SHARED
    am_radio.cpp
)

target_link_libraries(backends
PUBLIC
    audio
    dsp
    radio_sdr
)

target_include_directories(backends
PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_compile_features(radio_sdr
PUBLIC
    cxx_std_23
)

if(MSVC)
    target_compile_options(radio_sdr
    PRIVATE
        /W4
        /permissive
    )
else()
    target_compile_options(radio_sdr
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()