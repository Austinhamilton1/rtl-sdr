add_library(radio_sdr 
SHARED
    radio.cpp
)

target_link_libraries(radio_sdr PRIVATE ${RTLSDR_LIBRARIES})

target_include_directories(radio_sdr
PRIVATE
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