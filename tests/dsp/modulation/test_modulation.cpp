#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <stdexcept>

#include "dsp/modulation/demodulate.h"

namespace {
    constexpr float tolerance = 1e-4f;
}

TEST_CASE("AM demodulator returns the IQ envelope", "[demodulator]") {
    const float input_I[] = {3.0f, 0.0f, -4.0f, 0.0f};
    const float input_Q[] = {4.0f, 2.0f, 0.0f, -5.0f};

    float output[4]{};

    Demodulator::am_demodulate(output, input_I, input_Q, 4);

    const float expected[] = {5.0f, 2.0f, 4.0f, 5.0f};

    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_THAT(output[i],
            Catch::Matchers::WithinAbs(expected[i], tolerance));
    }
}

TEST_CASE("AM demodulator recovers a synthetic AM envelope", "[demodulator]") {
    constexpr size_t count = 256;
    constexpr float modulation_depth = 0.7f;
    constexpr float normalized_audio_frequency = 0.03125f;

    std::vector<float> input_I(count), input_Q(count);
    std::vector<float> output(count);

    for (size_t n = 0; n < count; ++n) {
        const float audio = std::cos(
            2.0f * std::numbers::pi_v<float> *
            normalized_audio_frequency * static_cast<float>(n)
        );

        input_I[n] = 1.0f + modulation_depth * audio;
        input_Q[n] = 0.0f;
    }

    Demodulator::am_demodulate(
        output.data(), input_I.data(), input_Q.data(), count
    );

    for (size_t n = 0; n < count; ++n) {
        REQUIRE_THAT(output[n],
            Catch::Matchers::WithinAbs(input_I[n], tolerance));
    }
}

TEST_CASE("AM demodulator rejects null pointers", "[demodulator]") {
    float input_I[2]{}, input_Q[2]{}, output[2]{};

    REQUIRE_THROWS_AS(
        Demodulator::am_demodulate(nullptr, input_I, input_Q, 2),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        Demodulator::am_demodulate(output, nullptr, input_Q, 2),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        Demodulator::am_demodulate(output, input_I, nullptr, 2),
        std::invalid_argument
    );
}