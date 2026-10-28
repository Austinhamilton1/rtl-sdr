#pragma once

class DCBlocker {
public:
    /*
     * Constructs a DC blocker with the specified cutoff frequency and sample rate.
     * @param cutoff_freq The cutoff frequency in Hz. Must be positive.
     * @param sample_rate The sample rate in Hz. Must be positive.
     */
    DCBlocker(float cutoff_freq, float sample_rate);

    /*
     * Processes a single input sample and returns the output sample with DC component removed.
     * @param input The input sample to process.
     * @return The output sample with DC component removed.
     */
    float process(float input);

private:
    float m_prev_input = 0.0f;
    float m_prev_output = 0.0f;
    float R;    // Close to 1; derived from the cutoff frequency and sample rate
};