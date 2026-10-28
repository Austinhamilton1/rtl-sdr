#pragma once

#include <cstddef>

struct BiquadCoeff {
    float b0, b1, b2, a1, a2;
};

class BiquadFilter {
public:
    BiquadFilter(BiquadCoeff coeff);

    /*
     * Filter complex streams.
     * @param *output_I Filtered In-Phase (real) signal.
     * @param *output_Q Filtered Quadrature (imag) singal.
     * @param *I Incoming In-Phase (real) singal.
     * @param *Q Incoming Quadrature (imag) signal.
     * @param num_samples Number of complex samples.
     */
    void filter(float *output_I, float *output_Q, const float *I, const float *Q, size_t num_samples);

    /*
     * Create lowpass filter coefficients.
     * @param sample_rate Sampling rate.
     * @param cutoff_freq Cutoff frequency.
     * @param Q Q factor.
     * @return BiquadCoeff struct containing the filter coefficients.
     */
    BiquadCoeff make_lowpass(double sample_rate, double cutoff_freq, double Q);
private:
    BiquadCoeff c;
    float x1_i = 0, x2_i = 0, y1_i = 0, y2_i = 0;
    float x1_q = 0, x2_q = 0, y1_q = 0, y2_q = 0;
};