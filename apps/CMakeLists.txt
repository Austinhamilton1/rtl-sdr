add_executable(radio radio.cpp)

target_link_libraries(radio
PRIVATE
    backends
    imgui
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