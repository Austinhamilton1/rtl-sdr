add_executable(modulation_test
    test_modulation.cpp
)

target_link_libraries(modulation_test
PRIVATE
    dsp
    Catch2::Catch2WithMain
)

include(Catch)

catch_discover_tests(modulation_test
PROPERTIES
    LABELS "rtl-sdr modulation"
)