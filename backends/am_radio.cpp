#include "am_radio.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "dsp/modulation/demodulate.h"

namespace {
    constexpr double butterworth = 0.707106781;
    constexpr double butterworth_1 = 0.5412;
    constexpr double butterworth_2 = 1.3066;

    float rms(float *samples, size_t sample_size) {
        float sum = 0.0f;
        for(size_t i = 0; i < sample_size; i++) {
            sum += samples[i] * samples[i] / sample_size;
        }

        return std::sqrt(sum);
    }

    size_t dsp_tune(void *userdata, float *I, float *Q, size_t complex_samples) {
        AMReceiver *am = static_cast<AMReceiver *>(userdata);

        am->tuner().process(
            I, Q, 
            I, Q, 
            complex_samples
        );

        return complex_samples;
    }

    size_t dsp_channel_filter(void *userdata, float *I, float *Q, size_t complex_samples) {
        AMReceiver *am = static_cast<AMReceiver *>(userdata);

        am->channel_filter().filter(
            I, Q, 
            I, Q, 
            complex_samples
        );

        return complex_samples;
    }
    
    size_t dsp_demodulate(void *userdata, float *I, float *Q, size_t complex_samples) {
        AMReceiver *am = static_cast<AMReceiver *>(userdata);

        Demodulator::am_demodulate(
            am->envelopes().data(), 
            I, Q, 
            complex_samples
        );

        return complex_samples;
    }
    
    size_t dsp_audio_filter(void *userdata, float *, float *, size_t envelopes) {
        AMReceiver *am = static_cast<AMReceiver *>(userdata);

        auto& samples = am->envelopes();

        am->audio_filter_1().filter(
            samples.data(), nullptr, 
            samples.data(), nullptr, 
            envelopes
        );

        am->audio_filter_2().filter(
            samples.data(), nullptr,
            samples.data(), nullptr,
            envelopes
        );

        return envelopes;
    }
    
    size_t dsp_resample(void *userdata, float *, float *, size_t envelopes) {
        AMReceiver *am = static_cast<AMReceiver *>(userdata);

        size_t resampled_size = am->resampler().process(
            am->resampled().data(), nullptr,
            am->envelopes().data(), nullptr,
            envelopes
        );

        return resampled_size;
    }
    
    size_t dsp_reshape(void *userdata, float *, float *, size_t resampled_size) {
        AMReceiver *am = static_cast<AMReceiver *>(userdata);

        auto& samples = am->resampled();
        const float gain = 100.0f;

        for(size_t i = 0; i < resampled_size; i++) {
            samples[i] = am->dc_blocker().process(samples[i]);
            samples[i] = std::clamp(
                samples[i] * gain,
                -1.0f,
                 1.0f
            );
        }

        return resampled_size;
    }
    
    size_t dsp_audio_write(void *userdata, float *, float *, size_t resampled_size) {
        AMReceiver *am = static_cast<AMReceiver *>(userdata);

        size_t samples = am->sink().write(
            am->resampled().data(),
            resampled_size
        );

        uint64_t past_samples = am->get_audio_samples();
        am->set_audio_samples(past_samples + samples);

        return resampled_size;
    }
}

void AMReceiver::initialize(const RadioConfig& config) {
    if (config.iq_sample_rate == 0 ||
        config.audio_sample_rate == 0) {
        throw std::invalid_argument("Sample rates must be positive");
    }

    if (config.channel_cutoff == 0 ||
        config.channel_cutoff >= config.iq_sample_rate / 2) {
        throw std::invalid_argument("Invalid channel cutoff");
    }

    if (config.audio_cutoff == 0 ||
        config.audio_cutoff >= config.iq_sample_rate / 2) {
        throw std::invalid_argument("Invalid audio cutoff");
    }

    m_config = config;
    set_audio_gain(m_config.audio_gain);

    // 1. Coonfigure RTL-SDR: RF center, sample rate,
    //    receive bandwidth, and gain.
    m_sdr.open();
    m_sdr.set_direct_sampling(2);
    m_sdr.set_center_frequency(config.center_frequency);
    m_sdr.set_sample_rate(config.iq_sample_rate);
    m_sdr.set_rx_bandwidth(config.channel_cutoff);
    m_sdr.set_gain_mode(true);
    m_sdr.reset_buffer();

    // 2. Design channel-filter coefficients using IQ rate.
    auto channel_c = BiquadFilter::make_lowpass(config.iq_sample_rate, config.channel_cutoff, butterworth);
    m_channel_filter = BiquadFilter(channel_c);

    // 3. Initialize tuner
    m_tuner.initialize(config.iq_sample_rate);

    // 4. Design audio-filter coefficients using the IQ rate.
    auto audio_c1 = BiquadFilter::make_lowpass(config.iq_sample_rate, config.audio_cutoff, butterworth_1);
    auto audio_c2 = BiquadFilter::make_lowpass(config.iq_sample_rate, config.audio_cutoff, butterworth_2);
    m_audio_filter_1 = BiquadFilter(audio_c1);
    m_audio_filter_2 = BiquadFilter(audio_c2);

    // 5. Configure the resampler for IQ rate -> audio rate.
    m_resampler = Sampler(config.iq_sample_rate, config.audio_sample_rate);

    // 6. Configure the blocker for audio rate
    m_dc_blocker.initialize(20.0f, config.audio_sample_rate);

    // 7. Allocate the buffer.
    m_buffer.initialize(256, config.iq_sample_rate);
    m_buffer.register_dsp(dsp_tune, 10 * NS_PER_MS);
    m_buffer.register_dsp(dsp_channel_filter, 10 * NS_PER_MS);
    m_buffer.register_dsp(dsp_demodulate, 10 * NS_PER_MS);
    m_buffer.register_dsp(dsp_audio_filter, 10 * NS_PER_MS);
    m_buffer.register_dsp(dsp_resample, 10 * NS_PER_MS);
    m_buffer.register_dsp(dsp_reshape, 10 * NS_PER_MS);
    m_buffer.register_dsp(dsp_audio_write, 10 * NS_PER_MS);
    
    // 7. Allocate buffer data
    const auto max_input_samples = detail::chunk_size;

    const auto estimated_output_samples =
        std::ceil(
            static_cast<double>(max_input_samples) *
            config.audio_sample_rate /
            config.iq_sample_rate
        );

    m_envelopes.resize(max_input_samples);
    m_resampled.resize(
        static_cast<size_t>(estimated_output_samples) + 2
    );

    // 9. Open the audio sink at 48 kHz mono.
    m_sink.initialize(config.audio_sample_rate);
    m_sink.open();
}

void AMReceiver::set_station_frequency(uint32_t station_hz) {
    const int64_t offset = 
        static_cast<int64_t>(m_config.center_frequency)
        - static_cast<int64_t>(station_hz);

    m_tuner.set_tuning_offset(static_cast<int32_t>(offset));
}

void AMReceiver::run() {
    m_running.store(true, std::memory_order_relaxed);

    m_producer = std::thread([&] {
        while(m_running.load(std::memory_order_relaxed)) {
            int64_t samples = m_sdr.read_samples(m_buffer, 4096);

            if(samples < 0) {
                std::cerr << "SDR read failed\n";
                request_stop();
                break;
            }

            if(samples == 0) {
                std::cerr << "SDR returned zero samples\n";
            }

            m_num_iq_samples.fetch_add(
                static_cast<uint64_t>(samples),
                std::memory_order_relaxed
            );
        }
    });

    m_consumer = std::thread([&] {
        while(m_running.load(std::memory_order_relaxed)) {
            if(m_buffer.run_dsp_callbacks(this) < 0) {
                request_stop();
                break;
            }
        }
    });
}

void AMReceiver::request_stop() noexcept {
    m_running.store(false, std::memory_order_relaxed);
}

void AMReceiver::stop() {
    request_stop();

    if(m_producer.joinable()) m_producer.join();
    if(m_consumer.joinable()) m_consumer.join();
}

void AMReceiver::set_audio_gain(float gain_db) noexcept {
    m_gain_linear.store(
        std::pow(10.0f, gain_db / 20.0f),
        std::memory_order_relaxed
    );
}