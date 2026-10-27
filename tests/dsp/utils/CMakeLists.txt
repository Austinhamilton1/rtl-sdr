add_executable(utils_test
    test_buffer.cpp
    test_sampler.cpp
)

target_link_libraries(utils_test
PRIVATE
    dsp
    Catch2::Catch2WithMain
)

include(Catch)

catch_discover_tests(utils_test
PROPERTIES
    LABELS "rtl-sdr utils"
)