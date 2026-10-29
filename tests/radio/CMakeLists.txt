add_executable(radio_test
    test_radio.cpp
)

target_link_libraries(radio_test
PRIVATE
    dsp
    radio_sdr
    Catch2::Catch2WithMain
)

include(Catch)

catch_discover_tests(radio_test
PROPERTIES
    LABELS "rtl-sdr radio"
)