#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include "audio/sink.h"
#include "dsp/filter/biquad.h"
#include "dsp/filter/tuner.h"
#include "dsp/modulation/blocker.h"
#include "dsp/utils/buffer.h"
#include "dsp/utils/sampler.h"
#include "radio/sdr/radio.h"

struct RadioConfig {
    uint32_t center_frequency; // in Hz
    uint64_t iq_sample_rate;   // in samples per second
    uint64_t audio_sample_rate; // in samples per second

    uint32_t channel_cutoff; // in Hz
    uint32_t audio_cutoff;   // in Hz

    float audio_gain; // in dB
};

class AMReceiver {
public:
    AMReceiver() = default;

    /*
     * Configure SDR, design filters and resampler, allocate buffer space, and open audio sink.
     * @param config Configuration parameters for the radio.
     */
    void initialize(const RadioConfig& config);

    /*
     * Set the frequency of the station through the tuner.
     * @param station_hz Absolute frequency of the broadcast.
     */
    void set_station_frequency(uint32_t station_hz);

    /*
     * Set the audio gain of the station.
     * @param gain Gain of the station.
     */
    void set_audio_gain(float gain_db) noexcept;

    /*
     * Actively run the AM receiver and play audio. 
     */
    void run();

    /*
     * Stop the AM receiver.
     */
    void stop();
    void request_stop() noexcept;

    /*
     * Get a reference to the internal audio sink.
     */
    AudioSink& sink() noexcept {
        return m_sink;
    }

    /*
     * Get a reference to the internal tuner.
     */
    Tuner& tuner() noexcept {
        return m_tuner;
    }

    /*
     * Get a reference to the internal channel filter.
     */
    BiquadFilter& channel_filter() noexcept {
        return m_channel_filter;
    }

    /*
     * Get a reference to the internal DC blocker.
     */
    DCBlocker& dc_blocker() noexcept {
        return m_dc_blocker;
    }

    /*
     * Get a reference to the internal audio filter.
     */
    BiquadFilter& audio_filter_1() noexcept {
        return m_audio_filter_1;
    }
    BiquadFilter& audio_filter_2() noexcept {
        return m_audio_filter_2;
    }

    /*
     * Get a reference to the internal resampler.
     */
    Sampler& resampler() noexcept {
        return m_resampler;
    }

    /*
     * Get a reference to the internal envelope data.
     */
    std::vector<float>& envelopes() noexcept {
        return m_envelopes;
    }

    /*
     * Get a reference to the internal resampled data.
     */
    std::vector<float>& resampled() noexcept {
        return m_resampled;
    }
    
    /*
     * Return the linear gain of the system.
     */
    float gain_linear() const noexcept {
        return m_gain_linear.load(std::memory_order_relaxed);
    }

    void set_audio_samples(uint64_t samples) {
        m_num_audio_samples.store(samples, std::memory_order_relaxed);
    }

    uint64_t get_audio_samples() {
        return m_num_audio_samples.load(std::memory_order_relaxed);
    }

    uint64_t get_iq_samples() {
        return m_num_iq_samples.load(std::memory_order_relaxed);
    }

    void set_rms(size_t i, float rms) {
        m_rms[i].store(rms, std::memory_order_relaxed);
    }

    float get_rms(size_t i) {
        return m_rms[i].load(std::memory_order_relaxed);
    }
    
private:
    SDRRadio m_sdr;
    AudioSink m_sink;

    Tuner m_tuner;
    BiquadFilter m_channel_filter;
    DCBlocker m_dc_blocker;
    BiquadFilter m_audio_filter_1;
    BiquadFilter m_audio_filter_2;
    Sampler m_resampler;

    Buffer m_buffer;

    RadioConfig m_config;

    std::atomic<bool> m_running{false};

    std::vector<float> m_envelopes;
    std::vector<float> m_resampled;

    std::thread m_producer;
    std::thread m_consumer;

    std::atomic<float> m_gain_linear{1.0f};

    std::atomic<uint64_t> m_num_iq_samples{0};
    std::atomic<uint64_t> m_num_audio_samples{0};

    std::array<std::atomic<float>, 8> m_rms{};
};