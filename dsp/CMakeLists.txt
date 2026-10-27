add_subdirectory(filter)
add_subdirectory(modulation)
add_subdirectory(utils)

add_library(dsp INTERFACE)

target_link_libraries(dsp
INTERFACE
    dsp_filter
    dsp_modulation
    dsp_utils
)

target_include_directories(dsp
INTERFACE
    ${PROJECT_SOURCE_DIR}
)