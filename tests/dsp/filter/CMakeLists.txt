add_executable(filter_test
    test_biquad.cpp
    test_tuner.cpp
)

target_link_libraries(filter_test
PRIVATE
    dsp
    Catch2::Catch2WithMain
)

include(Catch)

catch_discover_tests(filter_test
PROPERTIES
    LABELS "rtl-sdr filter"
)