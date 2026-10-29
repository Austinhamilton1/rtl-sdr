#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>

#include "dsp/filter/biquad.h"
#include "dsp/utils/sampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include<stdexcept>
#include <vector>

namespace {
    constexpr double pi = std::numbers::pi_v<double>;

    constexpr double sample_rate = 2'400'000.0;
    constexpr double cutoff_hz = 5'000.0;
    constexpr double q_factor = 1.0 / std::numbers::sqrt2;

    BiquadFilter make_test_filter() {
        return BiquadFilter(
            BiquadFilter::make_lowpass(
                sample_rate, cutoff_hz, q_factor
            )
        );
    }

    std::vector<float> make_sine(
        double frequency,
        size_t count,
        double rate = sample_rate
    ) {
        std::vector<float> samples(count);

        for (size_t i = 0; i < count; ++i) {
            const double t = static_cast<double>(i) / rate;
            samples[i] = static_cast<float>(
                std::sin(2.0 * pi * frequency * t)
            );
        }

        return samples;
    }

    double rms(
        const std::vector<float>& samples,
        size_t start = 0
    ) {
        double sum = 0.0;
        size_t count = 0;

        for (size_t i = start; i < samples.size(); ++i) {
            const double x = samples[i];
            sum += x * x;
            ++count;
        }

        return count > 0
            ? std::sqrt(sum / static_cast<double>(count))
            : 0.0;
    }
}

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

namespace {
    constexpr uint64_t input_rate = 2'400'000;
    constexpr uint64_t output_rate = 48'000;

    std::vector<float> make_tone(double frequency, size_t count) {
        std::vector<float> samples(count);

        for (size_t i = 0; i < count; ++i) {
            const double t = static_cast<double>(i) / input_rate;

            samples[i] = static_cast<float>(
                std::sin(2.0 * pi * frequency * t)
            );
        }

        return samples;
    }

    struct AliasTestResult {
        double input_rms;
        double output_rms;
        double attenuation_db;
        double alias_frequency_hz;
    };

    AliasTestResult measure_alias(double tone_hz) {
        // One tenth of a second gives 240,000 input samples.
        constexpr size_t input_count = 240'000;

        auto input = make_tone(tone_hz, input_count);
        std::vector<float> filtered(input_count);

        auto filter = BiquadFilter(
            BiquadFilter::make_lowpass(
                static_cast<double>(input_rate),
                cutoff_hz,
                q_factor
            )
        );

        filter.filter(
            filtered.data(), nullptr,
            input.data(), nullptr,
            input_count
        );

        Sampler sampler(input_rate, output_rate);

        std::vector<float> output(
            input_count / (input_rate / output_rate) + 2
        );

        const size_t output_count = sampler.process(
            output.data(), nullptr,
            filtered.data(), nullptr,
            filtered.size()
        );

        output.resize(output_count);

        // Discard the initial filter transient.
        constexpr size_t settling_output_samples = 500;

        REQUIRE(output.size() > settling_output_samples);

        const double input_level = rms(input, 1000);
        const double output_level = rms(output, settling_output_samples);

        const double attenuation_db =
            output_level > 0.0
                ? 20.0 * std::log10(output_level / input_level)
                : -INFINITY;

        // Fold the tone into the first Nyquist zone of the output.
        const double remainder =
            std::fmod(tone_hz, static_cast<double>(output_rate));

        const double alias_frequency =
            remainder <= output_rate / 2.0
                ? remainder
                : output_rate - remainder;

        return {
            input_level,
            output_level,
            attenuation_db,
            alias_frequency
        };
    }
} // namespace

TEST_CASE("5 kHz audio filter suppresses a 49 kHz alias",
          "[dsp][aliasing]") {
    const auto result = measure_alias(49'000.0);

    INFO("Input RMS: " << result.input_rms);
    INFO("Output RMS: " << result.output_rms);
    INFO("Attenuation (dB): " << result.attenuation_db);
    INFO("Alias frequency: " << result.alias_frequency_hz << " Hz");

    REQUIRE(result.alias_frequency_hz ==
            Catch::Approx(1'000.0).margin(0.01));

    // A second-order low-pass at 5 kHz should attenuate
    // a 49 kHz tone substantially.
    REQUIRE(result.attenuation_db < -35.0);
}

TEST_CASE("Cascaded biquads improve alias rejection",
          "[dsp][aliasing]") {
    constexpr size_t input_count = 240'000;

    auto input = make_tone(49'000.0, input_count);
    std::vector<float> stage1(input_count);
    std::vector<float> stage2(input_count);

    const auto coeff1 = BiquadFilter::make_lowpass(
        static_cast<double>(input_rate), cutoff_hz, 0.5411961
    );

    const auto coeff2 = BiquadFilter::make_lowpass(
        static_cast<double>(input_rate), cutoff_hz, 1.3065630
    );

    BiquadFilter filter1(coeff1);
    BiquadFilter filter2(coeff2);

    filter1.filter(
        stage1.data(), nullptr,
        input.data(), nullptr,
        input_count
    );

    filter2.filter(
        stage2.data(), nullptr,
        stage1.data(), nullptr,
        input_count
    );

    const double input_level = rms(input, 1000);
    const double output_level = rms(stage2, 1000);

    const double attenuation_db =
        20.0 * std::log10(output_level / input_level);

    INFO("Cascaded filter attenuation: " << attenuation_db << " dB");

    REQUIRE(attenuation_db < -60.0);
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

TEST_CASE("BiquadFilter impulse response is finite and decays",
          "[dsp][biquad]") {
    constexpr size_t count = 100'000;

    auto filter = make_test_filter();

    std::vector<float> input(count, 0.0f);
    std::vector<float> output(count, 0.0f);

    input[0] = 1.0f;

    filter.filter(
        output.data(), nullptr,
        input.data(), nullptr,
        count
    );

    float peak = 0.0f;

    for (float x : output) {
        REQUIRE(std::isfinite(x));
        peak = std::max(peak, std::abs(x));
    }

    REQUIRE(peak > 0.0f);

    // The impulse response should have decayed substantially
    // by the end of this block.
    REQUIRE(std::abs(output.back()) < peak * 1e-4f);
}

TEST_CASE("BiquadFilter passes frequencies below its cutoff",
          "[dsp][biquad]") {
    constexpr double frequency = 1'000.0;
    constexpr size_t count = 240'000;
    constexpr size_t settling_samples = 24'000;

    auto filter = make_test_filter();

    const auto input = make_sine(frequency, count);
    std::vector<float> output(count);

    filter.filter(
        output.data(), nullptr,
        input.data(), nullptr,
        count
    );

    const double input_rms = rms(input, settling_samples);
    const double output_rms = rms(output, settling_samples);
    const double gain = output_rms / input_rms;

    REQUIRE(std::isfinite(gain));

    // A 1 kHz tone should pass through a 5 kHz low-pass
    // filter with very little attenuation.
    REQUIRE(gain > 0.95);
    REQUIRE(gain < 1.05);
}

TEST_CASE("BiquadFilter attenuates frequencies above its cutoff",
          "[dsp][biquad]") {
    constexpr double frequency = 50'000.0;
    constexpr size_t count = 240'000;
    constexpr size_t settling_samples = 24'000;

    auto filter = make_test_filter();

    const auto input = make_sine(frequency, count);
    std::vector<float> output(count);

    filter.filter(
        output.data(), nullptr,
        input.data(), nullptr,
        count
    );

    const double input_rms = rms(input, settling_samples);
    const double output_rms = rms(output, settling_samples);
    const double gain = output_rms / input_rms;

    REQUIRE(std::isfinite(gain));

    // A 50 kHz tone is 10x the cutoff frequency.
    // A second-order low-pass should attenuate it substantially.
    REQUIRE(gain < 0.03);
}

TEST_CASE("BiquadFilter maintains independent I and Q state",
          "[dsp][biquad]") {
    constexpr size_t count = 48'000;

    auto complex_filter = make_test_filter();
    auto i_filter = make_test_filter();
    auto q_filter = make_test_filter();

    const auto input_i = make_sine(1'000.0, count);
    const auto input_q = make_sine(17'000.0, count);

    std::vector<float> output_i(count);
    std::vector<float> output_q(count);

    std::vector<float> expected_i(count);
    std::vector<float> expected_q(count);

    // Filter both channels using one complex filter.
    complex_filter.filter(
        output_i.data(), output_q.data(),
        input_i.data(), input_q.data(),
        count
    );

    // Filter each channel independently for comparison.
    i_filter.filter(
        expected_i.data(), nullptr,
        input_i.data(), nullptr,
        count
    );

    q_filter.filter(
        nullptr, expected_q.data(),
        nullptr, input_q.data(),
        count
    );

    for (size_t i = 0; i < count; ++i) {
        REQUIRE(output_i[i] == expected_i[i]);
        REQUIRE(output_q[i] == expected_q[i]);
    }
}

TEST_CASE("BiquadFilter remains stable over millions of samples",
          "[dsp][biquad]") {
    constexpr size_t count = 2'400'000;

    auto filter = make_test_filter();

    const auto input = make_sine(1'000.0, count);
    std::vector<float> output(count);

    filter.filter(
        output.data(), nullptr,
        input.data(), nullptr,
        count
    );

    for (float x : output) {
        REQUIRE(std::isfinite(x));
    }

    // After the initial transient, the output should remain
    // bounded and preserve the passband tone.
    const double output_rms = rms(output, 24'000);

    REQUIRE(std::isfinite(output_rms));
    REQUIRE(output_rms > 0.6);
    REQUIRE(output_rms < 0.8);
}
