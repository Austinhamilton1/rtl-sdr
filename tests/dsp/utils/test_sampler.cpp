#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <stdexcept>

#include "dsp/utils/sampler.h"

namespace {
    constexpr float tolerance = 1e-4f;
}

TEST_CASE("Sampler at equal rates preserves every sample", "[sampler]") {
    Sampler sampler(48'000, 48'000);

    const float input_I[] = {1.0f, 2.0f, 3.0f, 4.0f};
    const float input_Q[] = {-1.0f, -2.0f, -3.0f, -4.0f};

    float output_I[4]{}, output_Q[4]{};

    const size_t produced = sampler.process(
        output_I, output_Q, input_I, input_Q, 4
    );

    REQUIRE(produced == 4);

    for (size_t i = 0; i < produced; ++i) {
        REQUIRE(output_I[i] == input_I[i]);
        REQUIRE(output_Q[i] == input_Q[i]);
    }
}

TEST_CASE("Sampler performs integer-factor downsampling", "[sampler]") {
    Sampler sampler(1'000'000, 200'000);

    std::vector<float> input_I(100), input_Q(100);
    std::vector<float> output_I(100), output_Q(100);

    for (size_t i = 0; i < 100; ++i) {
        input_I[i] = static_cast<float>(i);
        input_Q[i] = -static_cast<float>(i);
    }

    const size_t produced = sampler.process(
        output_I.data(), output_Q.data(),
        input_I.data(), input_Q.data(), input_I.size()
    );

    REQUIRE(produced == 20);

    for (size_t i = 0; i < produced; ++i) {
        REQUIRE(output_I[i] == static_cast<float>(i * 5));
        REQUIRE(output_Q[i] == -static_cast<float>(i * 5));
    }
}

TEST_CASE("Sampler maintains rational rate across blocks", "[sampler]") {
    constexpr uint64_t input_rate = 1'000'000;
    constexpr uint64_t output_rate = 48'000;
    constexpr size_t count = 100'000;

    Sampler whole_sampler(input_rate, output_rate);
    Sampler split_sampler(input_rate, output_rate);

    std::vector<float> input_I(count), input_Q(count);
    std::vector<float> whole_I(count), whole_Q(count);
    std::vector<float> split_I(count), split_Q(count);

    for (size_t i = 0; i < count; ++i) {
        input_I[i] = static_cast<float>(i);
        input_Q[i] = -static_cast<float>(i);
    }

    const size_t whole_count = whole_sampler.process(
        whole_I.data(), whole_Q.data(),
        input_I.data(), input_Q.data(), count
    );

    const size_t first_count = split_sampler.process(
        split_I.data(), split_Q.data(),
        input_I.data(), input_Q.data(), 37'123
    );

    const size_t second_count = split_sampler.process(
        split_I.data() + first_count, split_Q.data() + first_count,
        input_I.data() + 37'123, input_Q.data() + 37'123,
        count - 37'123
    );

    const size_t split_count = first_count + second_count;

    REQUIRE(split_count == whole_count);

    for (size_t i = 0; i < whole_count; ++i) {
        REQUIRE(split_I[i] == whole_I[i]);
        REQUIRE(split_Q[i] == whole_Q[i]);
    }

    // The output count should track the rate ratio to within one sample.
    const double expected =
        static_cast<double>(count) * output_rate / input_rate;

    REQUIRE(std::abs(
        static_cast<double>(whole_count) - expected
    ) <= 1.0);
}

TEST_CASE("Sampler rejects invalid sample rates", "[sampler]") {
    REQUIRE_THROWS_AS(Sampler(0, 48'000), std::invalid_argument);
    REQUIRE_THROWS_AS(Sampler(48'000, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(Sampler(48'000, 96'000), std::invalid_argument);
}

TEST_CASE("Sampler rejects null pointers", "[sampler]") {
    Sampler sampler(48'000, 24'000);

    float input_I[4]{}, input_Q[4]{};
    float output_I[4]{}, output_Q[4]{};

    REQUIRE_THROWS_AS(
        sampler.process(nullptr, output_Q, input_I, input_Q, 4),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        sampler.process(output_I, nullptr, input_I, input_Q, 4),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        sampler.process(output_I, output_Q, nullptr, input_Q, 4),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        sampler.process(output_I, output_Q, input_I, nullptr, 4),
        std::invalid_argument
    );
}