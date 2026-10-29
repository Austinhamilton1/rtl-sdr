#include <catch2/catch_test_macros.hpp>

#include "radio/sdr/radio.h"

TEST_CASE("Radio can read from RTL", "[radio] [behavior]") {
    Buffer buffer(2, 2048000);
    SDRRadio radio;

    REQUIRE_NOTHROW(radio.open());
    REQUIRE_NOTHROW(radio.set_center_frequency(880'000));
    REQUIRE_NOTHROW(radio.set_gain_mode(true));
    REQUIRE_NOTHROW(radio.set_sample_rate(2048000));
    REQUIRE_NOTHROW(radio.reset_buffer());
     
    int64_t result = radio.read_samples(buffer, 4096);
    REQUIRE(result == 4096);
}