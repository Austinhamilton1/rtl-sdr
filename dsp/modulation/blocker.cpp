#include "blocker.h"

#include <cmath>
#include <stdexcept>

DCBlocker::DCBlocker(float cutoff_freq, float sample_rate) {
    if (cutoff_freq <= 0 || sample_rate <= 0) {
        throw std::invalid_argument("Cutoff frequency and sample rate must be positive");
    }
    R = 1.0f - (2.0f * M_PI * cutoff_freq / sample_rate);
}

float DCBlocker::process(float input) {
    float output = input - m_prev_input + R * m_prev_output;
    
    m_prev_input = input;
    m_prev_output = output;

    return output;
}

void DCBlocker::initialize(float cutoff_freq, float sample_rate) {
    if (cutoff_freq <= 0 || sample_rate <= 0) {
        throw std::invalid_argument("Cutoff frequency and sample rate must be positive");
    }
    R = 1.0f - (2.0f * M_PI * cutoff_freq / sample_rate);
}