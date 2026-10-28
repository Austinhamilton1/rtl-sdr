add_subdirectory(sdr)

add_library(sdr INTERFACE)

target_link_libraries(sdr
INTERFACE
    radio_sdr
)

target_include_directories(sdr
INTERFACE
    ${PROJECT_SOURCE_DIR}
)