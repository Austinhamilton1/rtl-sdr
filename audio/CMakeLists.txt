add_library(audio
SHARED
    sink.cpp
)

target_include_directories(audio
PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(audio PUBLIC PkgConfig::PIPEWIRE)

target_compile_features(audio
PUBLIC
    cxx_std_23
)

if(MSVC)
    target_compile_options(audio
    PRIVATE
        /W4
        /permissive
    )
else()
    target_compile_options(audio
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()