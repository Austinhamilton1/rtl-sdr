#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <stdexcept>

#include "dsp/filter/biquad.h"

namespace {
    constexpr float tolerance = 1e-4f;

    BiquadCoeff identity_coeff() {
        return BiquadCoeff{
            .b0 = 1.0f,
            .b1 = 0.0f,
            .b2 = 0.0f,
            .a1 = 0.0f,
            .a2 = 0.0f
        };
    }
}

TEST_CASE("Biquad rejects null pointers", "[biquad]") {
    BiquadFilter filter(identity_coeff());

    float input_I[2]{}, input_Q[2]{};
    float output_I[2]{}, output_Q[2]{};

    REQUIRE_THROWS_AS(
        filter.filter(nullptr, output_Q, input_I, input_Q, 2),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        filter.filter(output_I, nullptr, input_I, input_Q, 2),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        filter.filter(output_I, output_Q, nullptr, input_Q, 2),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        filter.filter(output_I, output_Q, input_I, nullptr, 2),
        std::invalid_argument
    );
}

TEST_CASE("Biquad identity filter preserves IQ", "[biquad]") {
    BiquadFilter filter(identity_coeff());

    const float input_I[] = {1.0f, -2.0f, 3.5f, 0.25f};
    const float input_Q[] = {4.0f, 2.0f, -1.0f, 0.5f};

    float output_I[4]{};
    float output_Q[4]{};

    filter.filter(output_I, output_Q, input_I, input_Q, 4);

    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_THAT(output_I[i],
            Catch::Matchers::WithinAbs(input_I[i], tolerance));
        REQUIRE_THAT(output_Q[i],
            Catch::Matchers::WithinAbs(input_Q[i], tolerance));
    }
}

TEST_CASE("Biquad applies FIR coefficients correctly", "[biquad]") {
    // y[n] = 0.5*x[n] + 0.5*x[n-1]
    BiquadFilter filter({
        .b0 = 0.5f,
        .b1 = 0.5f,
        .b2 = 0.0f,
        .a1 = 0.0f,
        .a2 = 0.0f
    });

    const float input_I[] = {2.0f, 4.0f, 6.0f};
    const float input_Q[] = {10.0f, 20.0f, 30.0f};

    float output_I[3]{};
    float output_Q[3]{};

    filter.filter(output_I, output_Q, input_I, input_Q, 3);

    const float expected_I[] = {1.0f, 3.0f, 5.0f};
    const float expected_Q[] = {5.0f, 15.0f, 25.0f};

    for (size_t i = 0; i < 3; ++i) {
        REQUIRE_THAT(output_I[i],
            Catch::Matchers::WithinAbs(expected_I[i], tolerance));
        REQUIRE_THAT(output_Q[i],
            Catch::Matchers::WithinAbs(expected_Q[i], tolerance));
    }
}

TEST_CASE("Biquad supports in-place filtering", "[biquad]") {
    BiquadFilter filter(identity_coeff());

    float I[] = {1.0f, 2.0f, 3.0f};
    float Q[] = {-1.0f, -2.0f, -3.0f};

    filter.filter(I, Q, I, Q, 3);

    REQUIRE(I[0] == 1.0f);
    REQUIRE(I[1] == 2.0f);
    REQUIRE(I[2] == 3.0f);

    REQUIRE(Q[0] == -1.0f);
    REQUIRE(Q[1] == -2.0f);
    REQUIRE(Q[2] == -3.0f);
}

TEST_CASE("Biquad retains state between blocks", "[biquad]") {
    BiquadCoeff coeff{
        .b0 = 0.5f,
        .b1 = 0.5f,
        .b2 = 0.0f,
        .a1 = 0.0f,
        .a2 = 0.0f
    };

    BiquadFilter whole_filter(coeff);
    BiquadFilter split_filter(coeff);

    const float input_I[] = {2.0f, 4.0f, 6.0f, 8.0f};
    const float input_Q[] = {1.0f, 3.0f, 5.0f, 7.0f};

    float whole_I[4]{}, whole_Q[4]{};
    float split_I[4]{}, split_Q[4]{};

    whole_filter.filter(whole_I, whole_Q, input_I, input_Q, 4);

    split_filter.filter(split_I, split_Q, input_I, input_Q, 2);
    split_filter.filter(
        split_I + 2, split_Q + 2,
        input_I + 2, input_Q + 2, 2
    );

    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_THAT(split_I[i],
            Catch::Matchers::WithinAbs(whole_I[i], tolerance));
        REQUIRE_THAT(split_Q[i],
            Catch::Matchers::WithinAbs(whole_Q[i], tolerance));
    }
}

TEST_CASE("Biquad coefficients are calculated correctly for lowpass filter", "[biquad]") {
    BiquadFilter filter(identity_coeff());

    double sample_rate = 48000.0;
    double cutoff_freq = 1000.0;
    double Q = 0.707;

    BiquadCoeff coeff = filter.make_lowpass(sample_rate, cutoff_freq, Q);

    // Expected coefficients calculated using standard formulas
    double omega = 2.0 * M_PI * cutoff_freq / sample_rate;
    double alpha = std::sin(omega) / (2.0 * Q);
    double cos_omega = std::cos(omega);

    double b0 = (1.0 - cos_omega) / 2.0;
    double b1 = 1.0 - cos_omega;
    double b2 = (1.0 - cos_omega) / 2.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cos_omega;
    double a2 = 1.0 - alpha;

    REQUIRE_THAT(coeff.b0,
        Catch::Matchers::WithinAbs(static_cast<float>(b0 / a0), tolerance));
    REQUIRE_THAT(coeff.b1,
        Catch::Matchers::WithinAbs(static_cast<float>(b1 / a0), tolerance));
    REQUIRE_THAT(coeff.b2,
        Catch::Matchers::WithinAbs(static_cast<float>(b2 / a0), tolerance));
    REQUIRE_THAT(coeff.a1,
        Catch::Matchers::WithinAbs(static_cast<float>(a1 / a0), tolerance));
    REQUIRE_THAT(coeff.a2,
        Catch::Matchers::WithinAbs(static_cast<float>(a2 / a0), tolerance));
}