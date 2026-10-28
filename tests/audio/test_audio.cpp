#include "audio/sink.h"

#include <cmath>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    constexpr uint32_t sample_rate = 48000;
    constexpr float frequency = 1000.0f;

    AudioSink sink(sample_rate);

    std::vector<float> samples(4800);

    double phase = 0.0;
    const double phase_step =
        2.0 * M_PI * frequency / sample_rate;

    // Generate 100 ms of the sine wave here to add some buffer.
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.15f * std::sin(phase);
        phase += phase_step;

        if (phase >= 2.0 * M_PI)
            phase -= 2.0 * M_PI;
    }

    sink.write(samples.data(), samples.size());
    sink.open();

    for(int block = 0; block < 30; block++) {
        for(float& sample : samples) {
            sample = 0.15f * std::sin(phase);
            phase += phase_step;

            if(phase >= 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
        }

        const size_t accepted =
            sink.write(samples.data(), samples.size());

        if(accepted != samples.size())
            std::cerr << "Audio buffer full; samples dropped\n";

        // This is just a simple test producer, not the final
        // timing mechanism for the SDR receiver.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(100)
        );
    }

    sink.close();

    std::cout << "Underruns: " << sink.underruns() << std::endl;
}