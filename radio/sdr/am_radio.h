#pragma once

#include <cstddef>
#include <cstdint>

#include "radio.h"
#include "dsp/filter/biquad.h"
#include "dsp/filter/tuner.h"
#include "dsp/modulation/blocker.h"
#include "dsp/utils/buffer.h"
#include "dsp/utils/sampler.h"

struct RadioConfig {
    uint32_t center_frequency; // in Hz
    uint64_t iq_sample_rate;   // in samples per second
    uint64_t audio_sample_rate; // in samples per second

    uint32_t channel_cutoff; // in Hz
    uint32_t audio_cutoff;   // in Hz

    int audio_gain; // in dB
};

class AMReceiver {
public:
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
     * Actively run the AM receiver and play audio. 
     */
    void run();

    /*
     * Stop the AM receiver.
     */
    void stop();
    
private:
    SDRRadio m_sdr;

    Tuner m_tuner;
    BiquadFilter m_channel_filter;
    DCBlocker m_dc_blocker;
    BiquadFilter m_audio_filter;
    Sampler m_resampler;

    Buffer m_buffer;

    RadioConfig m_config;

    bool m_running;
};