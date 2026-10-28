
#include "sampler.h"

#include <stdexcept>

Sampler::Sampler(
    uint64_t input_rate,
    uint64_t output_rate
)
    : m_input_rate(input_rate),
      m_output_rate(output_rate),
      m_phase(
        input_rate >= output_rate
            ? input_rate - output_rate
            : 0
      )
{
    if (input_rate == 0 || output_rate == 0) {
        throw std::invalid_argument(
            "Sample rates must be greater than zero."
        );
    }

    if (output_rate > input_rate) {
        throw std::invalid_argument(
            "Output rate must not exceed input rate."
        );
    }
}

void Sampler::reset() noexcept {
    m_phase = m_input_rate - m_output_rate;
}

size_t Sampler::process(
    float *output_I,
    float *output_Q,
    const float *I,
    const float *Q,
    size_t num_samples
) {
    if (num_samples == 0) {
        return 0;
    }

    size_t output_count = 0;
    uint64_t phase = m_phase;

    const uint64_t threshold = m_input_rate - m_output_rate;

    for(size_t i = 0; i < num_samples; i++) {
        if(phase >= threshold) {
            phase -= threshold;
            
            if(output_I != nullptr && I != nullptr) {
                output_I[output_count] = I[i];
            }

            if(output_Q != nullptr && Q != nullptr) {
                output_Q[output_count] = Q[i];
            }

            output_count++;
        } else {
            phase += m_output_rate;
        }
    }

    m_phase = phase;
    return output_count;
}
