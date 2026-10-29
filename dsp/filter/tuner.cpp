#include "tuner.h"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace {
    constexpr double pi = std::numbers::pi_v<double>;
    constexpr double two_pi = 2.0 * pi;
}

Tuner::Tuner(uint32_t sample_rate)
    : m_sample_rate(static_cast<double>(sample_rate))
{
    if(sample_rate == 0) {
        throw std::invalid_argument(
            "Sample rate must be greater than zero."
        );
    }
}

void Tuner::initialize(uint32_t sample_rate) {
    if(sample_rate == 0) {
        throw std::invalid_argument(
            "Sample rate must be greater than zero."
        );
    }
    
    m_sample_rate = static_cast<double>(sample_rate);
}

void Tuner::set_tuning_offset(int32_t offset_hz) noexcept {
    m_tuning_offset_hz.store(offset_hz, std::memory_order_relaxed);
}

void Tuner::process(
    float *output_I,
    float *output_Q,
    const float *I,
    const float *Q,
    size_t num_samples
) {
    if (num_samples == 0) {
        return;
    }

    if (output_I == nullptr || output_Q == nullptr ||
        I == nullptr || Q == nullptr) {
        throw std::invalid_argument(
            "IQ input and output pointers must not be null."
        );
    }

    const double requested_offset = static_cast<double>(
        m_tuning_offset_hz.load(std::memory_order_relaxed)
    );

    // Reduce the frequency modulo the sample rate so the phase
    // increment is bounded to [-pi, pi].
    const double offset = std::remainder(
        requested_offset,
        m_sample_rate
    );

    // Multiply by exp(-j * phase) to shift +offset toward DC.
    const double phase_increment =
        two_pi * offset / m_sample_rate;

    double phase = m_phase;

    for (size_t i = 0; i < num_samples; i++) {
        const double cos_phase = std::cos(phase);
        const double sin_phase = std::sin(phase);

        // Cache both inputs before writing outputs. This supports
        // pairwise in-place operation.
        const double in_i = I[i];
        const double in_q = Q[i];

        output_I[i] = static_cast<float>(
            in_i * cos_phase - in_q * sin_phase
        );

        output_Q[i] = static_cast<float>(
            in_i * sin_phase + in_q * cos_phase
        );

        phase += phase_increment;

        // Keep phase bounded even for long-running streams.
        if (phase >= pi) {
            phase -= two_pi;
        } else if (phase < -pi) {
            phase += two_pi;
        }
    }

    m_phase = phase;
}