#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>
#include <cmath>
#include <stdexcept>

#include "dsp/filter/tuner.h"
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

TEST_CASE("AM demodulator recovers a clean audio tone",
          "[dsp][am]") {
    constexpr uint32_t sample_rate = 2'400'000;
    constexpr double carrier_hz = 100'000.0;
    constexpr double audio_hz = 1'000.0;
    constexpr double modulation_depth = 0.5;
    constexpr double duration_seconds = 0.1;

    constexpr size_t num_samples =
        static_cast<size_t>(sample_rate * duration_seconds);

    constexpr double pi = std::numbers::pi_v<double>;

    std::vector<float> I(num_samples);
    std::vector<float> Q(num_samples);
    std::vector<float> envelope(num_samples);

    // Generate a clean complex AM signal.
    for (size_t n = 0; n < num_samples; ++n) {
        const double t = static_cast<double>(n) / sample_rate;

        const double modulation =
            1.0 + modulation_depth *
            std::cos(2.0 * pi * audio_hz * t);

        const double carrier_phase =
            2.0 * pi * carrier_hz * t;

        I[n] = static_cast<float>(
            modulation * std::cos(carrier_phase)
        );

        Q[n] = static_cast<float>(
            modulation * std::sin(carrier_phase)
        );
    }

    SECTION("Tuner shifts the carrier to DC") {
        Tuner tuner(sample_rate);
        tuner.set_tuning_offset(
            static_cast<int32_t>(carrier_hz)
        );

        tuner.process(
            I.data(), Q.data(),
            I.data(), Q.data(),
            num_samples
        );

        // After mixing, the carrier should be at DC.
        // Its magnitude remains the AM envelope.
        for (size_t i = 0; i < num_samples; ++i) {
            REQUIRE_THAT(std::sqrt(
                I[i] * I[i] + Q[i] * Q[i]
            ), Catch::Matchers::WithinAbs(
                1.0 + modulation_depth *
                std::cos(2.0 * pi * audio_hz *
                         static_cast<double>(i) / sample_rate),
                tolerance
            ));
        }
    }

    SECTION("Envelope detector recovers the expected tone") {
        Tuner tuner(sample_rate);
        tuner.set_tuning_offset(
            static_cast<int32_t>(carrier_hz)
        );

        tuner.process(
            I.data(), Q.data(),
            I.data(), Q.data(),
            num_samples
        );

        Demodulator::am_demodulate(
            envelope.data(),
            I.data(), Q.data(),
            num_samples
        );

        // Ignore initial samples for a future stateful pipeline.
        constexpr size_t start = 1000;
        const size_t count = num_samples - start;

        double mean = 0.0;
        for (size_t i = start; i < num_samples; ++i) {
            mean += envelope[i];
        }
        mean /= static_cast<double>(count);

        double cosine_sum = 0.0;
        double sine_sum = 0.0;
        double squared_sum = 0.0;

        for (size_t i = start; i < num_samples; ++i) {
            const double t = static_cast<double>(i) / sample_rate;
            const double x = envelope[i] - mean;
            const double phase = 2.0 * pi * audio_hz * t;

            cosine_sum += x * std::cos(phase);
            sine_sum += x * std::sin(phase);
            squared_sum += x * x;
        }

        const double recovered_amplitude =
            2.0 * std::hypot(cosine_sum, sine_sum) /
            static_cast<double>(count);

        const double recovered_rms =
            std::sqrt(squared_sum / static_cast<double>(count));

        REQUIRE_THAT(mean, Catch::Matchers::WithinAbs(1.0, 0.01));
        REQUIRE_THAT(recovered_amplitude,
                Catch::Matchers::WithinAbs(modulation_depth, 0.01));
        REQUIRE_THAT(recovered_rms,
                Catch::Matchers::WithinAbs(modulation_depth / std::sqrt(2.0), 0.01));
    }
}