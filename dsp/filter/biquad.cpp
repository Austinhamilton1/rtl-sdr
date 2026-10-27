#include "biquad.h"

#include <stdexcept>

BiquadFilter::BiquadFilter(BiquadCoeff coeff)
    : c(coeff) {}

void BiquadFilter::filter(
    float* output_I,
    float* output_Q,
    const float* I,
    const float* Q,
    size_t num_samples
) {
    if(
        output_I == nullptr || output_Q == nullptr
        || I == nullptr || Q == nullptr
    ) {
        throw std::invalid_argument("Pointers cannot be null");
    }
    
    for (size_t i = 0; i < num_samples; ++i) {
        // Cache inputs before writing outputs; supports in-place filtering.
        const float input_i = I[i];
        const float input_q = Q[i];

        const float filtered_i =
            c.b0 * input_i
            + c.b1 * x1_i
            + c.b2 * x2_i
            - c.a1 * y1_i
            - c.a2 * y2_i;

        const float filtered_q =
            c.b0 * input_q
            + c.b1 * x1_q
            + c.b2 * x2_q
            - c.a1 * y1_q
            - c.a2 * y2_q;

        x2_i = x1_i;
        x1_i = input_i;
        y2_i = y1_i;
        y1_i = filtered_i;

        x2_q = x1_q;
        x1_q = input_q;
        y2_q = y1_q;
        y1_q = filtered_q;

        output_I[i] = filtered_i;
        output_Q[i] = filtered_q;
    }
}
