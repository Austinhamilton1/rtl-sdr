#include "demodulate.h"

#include <cmath>
#include <stdexcept>
#include <vector>

void Demodulator::am_demodulate(
    float *output,
    const float *I,
    const float *Q,
    size_t num_samples
) {
    if(output == nullptr || I == nullptr || Q == nullptr) {
        throw std::invalid_argument("Pointers cannot be null.");
    }
    
    for(size_t i = 0; i < num_samples; i++) {
        // Calculate the envelope (magnitude of the IQ sample).
        output[i] = std::sqrt(
            I[i] * I[i]
            + Q[i] * Q[i]
        );
    }
}