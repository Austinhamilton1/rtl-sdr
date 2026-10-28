#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "dsp/utils/buffer.h"

namespace {
    struct Recorder {
    public:
        explicit Recorder(size_t n) {
            for(size_t i = 0; i < n; i++) {
                m_table[static_cast<float>(i)] = {0};
            }
        }

        void record(float x) {
            m_table[x].fetch_add(1, std::memory_order_relaxed);
        }

        bool check_count(size_t n) {
            for(const auto& it : m_table) {
                if(it.second != n)
                    return false;
            }

            return true;
        }

    private:
        std::unordered_map<float, std::atomic<size_t>> m_table;  
    };

    size_t consume_and_record(void *user_data, float *I, float *Q, size_t complex_samples) {
        Recorder *recorder = (Recorder *)user_data;

        for(size_t i = 0; i < complex_samples; i++) {
            recorder->record(I[i]);
            recorder->record(Q[i]);
        }

        return complex_samples;
    }

    size_t consume_error(void *user_data, float *I, float *Q, size_t complex_samples) {
        throw std::runtime_error("error");
        return complex_samples;
    }

    size_t consume_after_second(void *user_data, float *I, float *Q, size_t complex_samples) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return complex_samples;
    }

    struct magnitudes {
        float *mags;
        size_t offset;
    };

    size_t consume_realistic(void *user_data, float *I, float *Q, size_t complex_samples) {
        double a = 0.75;
        double inverse_a = 0.25;

        magnitudes *mags = (magnitudes *)user_data;

        for(size_t i = 0; i < complex_samples; i++) {
            size_t mag_idx = mags->offset + i;
            mags->mags[mag_idx] = std::sqrt(
                I[i] * I[i]
                + Q[i] * Q[i]
            );
        }

        mags->offset += complex_samples;

        return complex_samples;
    }
}

TEST_CASE("Buffer can be instantiated", "[buffer] [setup]") {
    REQUIRE_NOTHROW(Buffer(1, 0));

    size_t max_chunks = 16384;

    REQUIRE_NOTHROW(
        Buffer(max_chunks, 0)
    );
}

TEST_CASE("Buffer rejects invalid chunk sizes", "[buffer] [setup]") {
    REQUIRE_THROWS_AS(Buffer(0, 0), std::invalid_argument);

    size_t max_chunks = 16384;

    REQUIRE_THROWS_AS(
        Buffer(max_chunks + 1, 0), 
        std::length_error
    );
}

TEST_CASE("Registering a null callback fails", "[buffer] [setup]") {
    Buffer buffer(1, 0);

    REQUIRE_THROWS_AS(
        buffer.register_dsp(nullptr, 0),
        std::invalid_argument
    );
}

TEST_CASE("Filling the buffer stops pushes", "[buffer] [behavior]") {
    Buffer buffer(1, 1000);

    for(size_t i = 0; i < 4096; i++) {
        REQUIRE(buffer.push(0.0f, 0.0f) == 0);
    }

    for(size_t i = 0; i < 100; i++) {
        REQUIRE(buffer.push(0.0f, 0.0f) < 0);
    }
}

TEST_CASE("Consuming the buffer consumes full chunk", "[buffer] [behavior]") {
    Buffer buffer(1, 1000);

    buffer.register_dsp(consume_and_record, 100);

    for(size_t i = 0; i < 4096; i++) {
        REQUIRE(buffer.push(static_cast<float>(i), static_cast<float>(i)) == 0);
    }

    Recorder recorder(4096);
    buffer.run_dsp_callbacks(&recorder);

    REQUIRE(recorder.check_count(2));

    for(size_t i = 0; i < 4096; i++) {
        REQUIRE(buffer.push(0.0f, 0.0f) == 0);
    }
}

TEST_CASE("Callback errors bubble up", "[buffer] [behavior]") {
    Buffer buffer(1, 1000);

    buffer.register_dsp(consume_error, 100);

    for(size_t i = 0; i < 4096; i++) {
        buffer.push(0.0f, 0.0f);
    }

    REQUIRE(buffer.run_dsp_callbacks(nullptr) < 0);
}

TEST_CASE("Long running consumer copies and relinquishes", "[buffer] [parallel]") {
    Buffer buffer(1, 4096);

    buffer.register_dsp(consume_after_second, 1'000'000'000);

    for(size_t i = 0; i < 4096; i++) {
        REQUIRE(buffer.push(0.0f, 0.0f) == 0);
    }

    size_t successful_pushes = 0;
    int callback_result = 0;

    std::thread producer([&buffer, &successful_pushes] {
        // Give sufficient time for copy but not enough time for consumer
        std::this_thread::sleep_for(std::chrono::milliseconds(900));

        for(size_t i = 0; i < 4096; i++) {
            if(buffer.push(0.0f, 0.0f) == 0) {
                successful_pushes++;
            }
        }
    });

    std::thread consumer([&buffer, &callback_result] {
        callback_result = buffer.run_dsp_callbacks(nullptr);
    });

    producer.join();
    consumer.join();

    REQUIRE(successful_pushes == 4096);
    REQUIRE(callback_result == 0);
}

TEST_CASE("Producer and consumer can simultaneously work", "[buffer] [parallel]") {
    Buffer buffer(128, 1'000'000);

    // Initialize realistic workload with a 0.05 second real-time hint
    buffer.register_dsp(consume_realistic, 50'000'000);

    const size_t samples = 1'048'576;

    magnitudes mags = {
        .mags = new float[samples],
        .offset = 0,
    };
    std::memset(mags.mags, 0, samples * sizeof(float));

    constexpr int64_t target = 4096 * 1000;
    size_t failed_pushes = 0;

    std::thread producer([&buffer, &failed_pushes] {
        // Produce a unit complex signal at 1 MHz sample rate
        for(size_t i = 0; i < samples; i += 4096) {
            auto start = std::chrono::steady_clock::now();
           
            for(size_t j = 0; j < 4096; j++) {
                if(buffer.push(1.0f, 1.0f) < 0) {
                    failed_pushes++;
                }
            }

            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            
            auto wait = target - elapsed;
            if(wait > 0) {
                std::this_thread::sleep_for(
                    std::chrono::nanoseconds(wait)
                );
            }
        }
    });

    std::thread consumer([&buffer, &mags] {
        while(mags.offset < samples) {
            if(buffer.run_dsp_callbacks(&mags) < 0) {
                break;
            }
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(failed_pushes == 0);
    REQUIRE(mags.offset == samples);

    const float sqrt_2 = std::sqrt(2.0f);

    for(size_t i = 0; i < mags.offset; i++) {
        REQUIRE_THAT(
            mags.mags[i],
            Catch::Matchers::WithinAbs(
                sqrt_2,
                0.0001
            )
        );
    }

    delete[] mags.mags;
}