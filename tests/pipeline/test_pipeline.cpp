#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <thread>
#include <vector>

// Adjust these include paths to match your project.
#include "audio/sink.h"
#include "backends/am_radio.h"
#include "dsp/filter/tuner.h"
#include "dsp/filter/biquad.h"
#include "dsp/utils/sampler.h"
#include "dsp/modulation/blocker.h"
#include "dsp/modulation/demodulate.h"

int main() {
    constexpr uint32_t iq_rate = 2'400'000;
    constexpr uint32_t audio_rate = 48'000;

    constexpr double carrier_hz = 100'000.0;
    constexpr double tone_hz = 1'000.0;
    constexpr double duration_seconds = 10.0;

    constexpr float modulation_depth = 0.5f;
    constexpr float carrier_amplitude = 0.8f;
    constexpr float audio_gain = 1.5f;

    constexpr size_t chunk_size = 24'000; // 10 ms at 2.4 MHz

    constexpr double pi = std::numbers::pi_v<double>;
    constexpr double two_pi = 2.0 * pi;
    constexpr float butterworth_q = 0.70710678f;

    const auto make_lowpass = [](double rate, double cutoff, double q) {
        return BiquadFilter(
            BiquadFilter::make_lowpass(rate, cutoff, q));
    };

    auto check_finite = [](const char* stage,
                        const float* samples,
                        size_t count) {
        for (size_t i = 0; i < count; ++i) {
            if (!std::isfinite(samples[i])) {
                std::cerr << "Non-finite sample at "
                        << stage << ", index " << i
                        << ": " << samples[i] << '\n';
                return;
            }
        }
    };

    // After each audio filter:

    // After sampler.process():

    // Configure the tuner to shift the synthetic carrier to baseband.
    Tuner tuner;
    tuner.initialize(iq_rate);
    tuner.set_tuning_offset(
        static_cast<int32_t>(carrier_hz));

    // RF/channel filter: preserve the demodulated station bandwidth.
    auto channel_filter =
        make_lowpass(iq_rate, 10'000.0, butterworth_q);

    auto coeff = channel_filter.c;
    std::cout << "b0 = " << coeff.b0 << '\n'
              << "b1 = " << coeff.b1 << '\n'
              << "b2 = " << coeff.b2 << '\n'
              << "a1 = " << coeff.a1 << '\n'
              << "a2 = " << coeff.a2 << '\n';

    // Two cascaded low-pass sections for stronger anti-alias filtering.
    auto audio_filter_1 =
        make_lowpass(iq_rate, 5'000.0, 0.5411961f);

    auto audio_filter_2 =
        make_lowpass(iq_rate, 5'000.0, 1.3065630f);

    DCBlocker dc_blocker(20.0f, static_cast<float>(iq_rate));
    Sampler sampler(iq_rate, audio_rate);
    AudioSink sink(audio_rate);

    // Synthetic complex IQ and intermediate buffers.
    std::vector<float> I(chunk_size);
    std::vector<float> Q(chunk_size);
    std::vector<float> envelopes(chunk_size);

    // 10 ms at 48 kHz gives 480 samples; leave a little margin.
    std::vector<float> audio(chunk_size / 50 + 16);

    const size_t total_chunks = static_cast<size_t>(
        duration_seconds * iq_rate / chunk_size);

    uint64_t sample_index = 0;
    size_t total_audio_samples = 0;

    sink.open();

    const auto start = std::chrono::steady_clock::now();

    for (size_t chunk = 0; chunk < total_chunks; ++chunk) {
        // 1. Generate a 1 kHz AM signal at a 100 kHz carrier offset.
        //
        // The negative Q sign matches the frequency convention described
        // in your current Tuner implementation.
        for (size_t i = 0; i < chunk_size; ++i) {
            const double t =
                static_cast<double>(sample_index + i) / iq_rate;

            const float envelope = carrier_amplitude * static_cast<float>(
                1.0 + modulation_depth *
                    std::cos(two_pi * tone_hz * t));

            const double carrier_phase = two_pi * carrier_hz * t;

            I[i] = envelope * static_cast<float>(
                std::cos(carrier_phase));

            Q[i] = -envelope * static_cast<float>(
                std::sin(carrier_phase));
        }

        sample_index += chunk_size;

        // 2. Tune the carrier to baseband.
        tuner.process(
            I.data(), Q.data(),
            I.data(), Q.data(), chunk_size);

        check_finite("tuner", I.data(), chunk_size);

        double before_power = 0.0;

        for (size_t i = 0; i < chunk_size; ++i) {
            before_power +=
                double(I[i]) * I[i] +
                double(Q[i]) * Q[i];
        }

        channel_filter.filter(
            I.data(), Q.data(),
            I.data(), Q.data(),
            chunk_size
        );

        check_finite("channel_filter", I.data(), chunk_size);

        double after_power = 0.0;

        for (size_t i = 0; i < chunk_size; ++i) {
            after_power +=
                double(I[i]) * I[i] +
                double(Q[i]) * Q[i];
        }

        if (chunk % 100 == 0) {
            std::cout
                << "IQ RMS before channel filter: "
                << std::sqrt(before_power / chunk_size)
                << ", after: "
                << std::sqrt(after_power / chunk_size)
                << '\n';
        }

        // 4. AM envelope detection.
        Demodulator::am_demodulate(
            envelopes.data(),
            I.data(), Q.data(), chunk_size);

        check_finite("demodulator", envelopes.data(), chunk_size);

        double envelope_sum = 0.0;
        float envelope_min = envelopes[0];
        float envelope_max = envelopes[0];

        for (size_t i = 0; i < chunk_size; ++i) {
            envelope_sum += envelopes[i];
            envelope_min = std::min(envelope_min, envelopes[i]);
            envelope_max = std::max(envelope_max, envelopes[i]);
        }

        // 5. Remove the envelope's DC component.
        for (float& sample : envelopes) {
            sample = dc_blocker.process(sample);
        }

        // After the DC blocker:
        check_finite("DC blocker", envelopes.data(), chunk_size);

        // 6. Low-pass filter before decimation.
        audio_filter_1.filter(
            envelopes.data(), nullptr,
            envelopes.data(), nullptr, chunk_size);
        
        check_finite("audio filter 1", envelopes.data(), chunk_size);

        audio_filter_2.filter(
            envelopes.data(), nullptr,
            envelopes.data(), nullptr, chunk_size);

        check_finite("audio filter 2", envelopes.data(), chunk_size);

        // 7. Decimate from 2.4 MHz to 48 kHz.
        const size_t audio_count = sampler.process(
            audio.data(), nullptr,
            envelopes.data(), nullptr, chunk_size);
        
        check_finite("sampler", audio.data(), audio_count);

        // 8. Scale and protect the audio output from clipping.
        for (size_t i = 0; i < audio_count; ++i) {
            audio[i] = std::clamp(
                audio[i] * audio_gain, -1.0f, 1.0f);
        }

        double sum_sq = 0.0;
        float peak = 0.0f;

        for (size_t i = 0; i < audio_count; ++i) {
            sum_sq += static_cast<double>(audio[i]) * audio[i];
            peak = std::max(peak, std::abs(audio[i]));
        }

        const double rms = audio_count > 0
            ? std::sqrt(sum_sq / audio_count)
            : 0.0;

        if (chunk % 100 == 0) {
            std::cout << "Audio RMS: " << rms
                    << ", peak: " << peak << '\n';

            std::cout << "Tuned IQ RMS: "
                      << std::sqrt(before_power / chunk_size) << '\n';

            std::cout << "Envelope mean: "
                      << envelope_sum / chunk_size
                      << ", min: " << envelope_min
                      << ", max: " << envelope_max << '\n';
        }

        // 9. Play the generated audio.
        const size_t written = sink.write(audio.data(), audio_count);
        total_audio_samples += written;

        if (written != audio_count) {
            std::cerr << "Audio short write: " << written
                      << " / " << audio_count << '\n';
        }

        // Pace processing to approximately real time.
        const auto deadline = start + std::chrono::duration_cast<
            std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(
                    static_cast<double>(sample_index) / iq_rate));

        std::this_thread::sleep_until(deadline);
    }

    sink.close();

    std::cout << "Generated IQ samples: " << sample_index << '\n';
    std::cout << "Audio samples written: " << total_audio_samples << '\n';
    std::cout << "Expected audio samples: "
              << duration_seconds * audio_rate << '\n';

    return 0;
}