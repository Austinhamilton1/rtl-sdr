#pragma once

#include <cstddef>
#include <cstdint>

class Demodulator {
public:
    /*
     * Demodulate a AM (amplitude-modulated) signal.
     * @param *output Real demodulated values from teh signal.
     * @param *I In-Phase (real) portion of sample.
     * @param *Q Quadrature (imaginary) portion of sample.
     * @param num_samples Number of samples to demodulate.
     */
    static void am_demodulate(float *output, const float *I, const float *Q, size_t num_samples);
};