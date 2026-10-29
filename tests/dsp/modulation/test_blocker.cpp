#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <stdexcept>

#include "dsp/modulation/blocker.h"

namespace {
    constexpr float tolerance = 1e-4f;
}

TEST_CASE("Blocker rejects non-positive cutoff frequency and sample rate", "[blocker]") {
    REQUIRE_THROWS_AS(DCBlocker(-1.0f, 44100.0f), std::invalid_argument);
    REQUIRE_THROWS_AS(DCBlocker(100.0f, -44100.0f), std::invalid_argument);
    REQUIRE_THROWS_AS(DCBlocker(0.0f, 44100.0f), std::invalid_argument);
    REQUIRE_THROWS_AS(DCBlocker(100.0f, 0.0f), std::invalid_argument);
}

TEST_CASE("Blocker processes a DC signal correctly", "[blocker]") {
    constexpr float cutoff_freq = 10.0f;
    constexpr float sample_rate = 44100.0f;

    DCBlocker blocker(cutoff_freq, sample_rate);

    const float input[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float output[4]{};

    for (size_t i = 0; i < 4; ++i) {
        output[i] = blocker.process(input[i]);
    }

    const float expected[] = {1.0f, 0.9999f, 0.9998f, 0.9997f};

    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_THAT(output[i],
            Catch::Matchers::WithinAbs(expected[i], tolerance));
    }
}