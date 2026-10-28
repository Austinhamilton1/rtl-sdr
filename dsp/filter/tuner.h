#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "biquad.h"

class Tuner {
public:
    /*
     * Initialize a tuner.
     * @param sample_rate Sampling rate in Hz.
     */
    Tuner(uint32_t sample_rate);

    /*
     * Initialize a tuner.
     * @param sample_rate Sampling rate in Hz.
     */
    void initialize(uint32_t sample_rate);

    /*
     * Offset tuning. If center_frequency == 1000 kHz and you want
     * 1010 kHz, input 10_000. (Supports negative frequencies).
     * @param offset_hz Offset in Hz.
     */
    void set_tuning_offset(int32_t offset_hz) noexcept;

    /*
     * Process a block of incoming data to tune to the Tuner's frequency.
     * @param *output_I Filtered In-Phase (real) signal.
     * @param *output_Q Filtered Quadrature (imag) signal.
     * @param *I Incoming In-Phase (real) signal.
     * @param *Q Incoming Quadrature (imag) signal.
     * @param num_samples Number of complex samples to process.
     */
    void process(float *output_I, float *output_Q, const float *I, const float *Q, size_t num_samples);
private:
    double m_sample_rate;
    std::atomic<int32_t> m_tuning_offset_hz{0};
    
    double m_phase = 0.0;
};