
#pragma once

#include <cstddef>
#include <cstdint>

class Sampler {
public:
    /*
     * This class only supports downsampling:
     * 0 < output_rate <= input_rate.
     * @param input_rate Input sample rate in Hz.
     * @param output_rate Desired output sample rate in Hz.
     */
    Sampler(uint64_t input_rate, uint64_t output_rate);

    /*
     * Downsample one block.
     *
     *
     * The output buffers must have capacity for at least
     * ceil(num_samples * output_rate / input_rate) samples.
     *
     * Call repeatedly on successive blocks. Sampling phase is
     * retained between calls.
     *
     * Input and output arrays may alias pairwise only when
     * output positions cannot overwrite future input samples.
     * For simplicity, use separate input/output buffers.
     * 
     * @return Number of output samples written.
     */
    size_t process(
        float* output_I,
        float* output_Q,
        const float* I,
        const float* Q,
        size_t num_samples
    );

    // Reset the sampling phase to its initial alignment.
    void reset() noexcept;

private:
    const uint64_t m_input_rate;
    const uint64_t m_output_rate;

    // Always less than m_input_rate.
    uint64_t m_phase;
};
