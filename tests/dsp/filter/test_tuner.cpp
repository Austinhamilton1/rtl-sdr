#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <stdexcept>

#include "dsp/filter/tuner.h"

namespace {
    constexpr float tolerance = 1e-4f;
}

TEST_CASE("Tuner with zero offset preserves IQ", "[tuner]") {
    Tuner tuner(48'000);

    const float input_I[] = {1.0f, 0.0f, -1.0f, 0.5f};
    const float input_Q[] = {0.0f, 1.0f, 0.0f, -0.5f};

    float output_I[4]{}, output_Q[4]{};

    tuner.process(output_I, output_Q, input_I, input_Q, 4);

    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_THAT(output_I[i],
            Catch::Matchers::WithinAbs(input_I[i], tolerance));
        REQUIRE_THAT(output_Q[i],
            Catch::Matchers::WithinAbs(input_Q[i], tolerance));
    }
}

TEST_CASE("Tuner shifts a matching complex tone to DC", "[tuner]") {
    constexpr uint64_t sample_rate = 48'000;
    constexpr int64_t tone_hz = 3'000;
    constexpr size_t count = 256;

    Tuner tuner(sample_rate);
    tuner.set_tuning_offset(tone_hz);

    std::vector<float> input_I(count), input_Q(count);
    std::vector<float> output_I(count), output_Q(count);

    for (size_t n = 0; n < count; ++n) {
        const double phase =
            2.0 * std::numbers::pi_v<double> *
            static_cast<double>(tone_hz) * n / sample_rate;

        input_I[n] = static_cast<float>(std::cos(phase));
        input_Q[n] = static_cast<float>(std::sin(phase));
    }

    tuner.process(
        output_I.data(), output_Q.data(),
        input_I.data(), input_Q.data(), count
    );

    for (size_t i = 0; i < count; ++i) {
        REQUIRE_THAT(output_I[i],
            Catch::Matchers::WithinAbs(1.0, tolerance));
        REQUIRE_THAT(output_Q[i],
            Catch::Matchers::WithinAbs(0.0, tolerance));
    }
}

TEST_CASE("Tuner preserves oscillator phase across blocks", "[tuner]") {
    constexpr uint64_t sample_rate = 48'000;
    constexpr int64_t tone_hz = 1'000;
    constexpr size_t count = 300;

    Tuner whole_tuner(sample_rate);
    Tuner split_tuner(sample_rate);

    whole_tuner.set_tuning_offset(tone_hz);
    split_tuner.set_tuning_offset(tone_hz);

    std::vector<float> input_I(count), input_Q(count);
    std::vector<float> whole_I(count), whole_Q(count);
    std::vector<float> split_I(count), split_Q(count);

    for (size_t n = 0; n < count; ++n) {
        const double phase =
            2.0 * std::numbers::pi_v<double> *
            static_cast<double>(tone_hz) * n / sample_rate;

        input_I[n] = static_cast<float>(std::cos(phase));
        input_Q[n] = static_cast<float>(std::sin(phase));
    }

    whole_tuner.process(
        whole_I.data(), whole_Q.data(),
        input_I.data(), input_Q.data(), count
    );

    split_tuner.process(
        split_I.data(), split_Q.data(),
        input_I.data(), input_Q.data(), 123
    );

    split_tuner.process(
        split_I.data() + 123, split_Q.data() + 123,
        input_I.data() + 123, input_Q.data() + 123,
        count - 123
    );

    for (size_t i = 0; i < count; ++i) {
        REQUIRE_THAT(split_I[i],
            Catch::Matchers::WithinAbs(whole_I[i], tolerance));
        REQUIRE_THAT(split_Q[i],
            Catch::Matchers::WithinAbs(whole_Q[i], tolerance));
    }
}

TEST_CASE("Tuner rejects null pointers", "[tuner]") {
    Tuner tuner(48'000);

    float input_I[2]{}, input_Q[2]{};
    float output_I[2]{}, output_Q[2]{};

    REQUIRE_THROWS_AS(
        tuner.process(nullptr, output_Q, input_I, input_Q, 2),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        tuner.process(output_I, nullptr, input_I, input_Q, 2),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        tuner.process(output_I, output_Q, nullptr, input_Q, 2),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        tuner.process(output_I, output_Q, input_I, nullptr, 2),
        std::invalid_argument
    );
}