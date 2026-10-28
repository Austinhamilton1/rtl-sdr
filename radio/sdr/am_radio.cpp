#include "am_radio.h"

void AMReceiver::initialize(const RadioConfig& config) {
    // 1. Coonfigure RTL-SDR: RF center, sample rate,
    //    receive bandwidth, and gain.
    // 2. Design channel-filter coefficients using IQ rate.
    // 3. Design audio-filter coefficients using the
    //    post-demodulation rate.
    // 4. Configure the resampler for IQ rate -> 48 KHz.
    // 5. Allocate the buffer.
    // 6. Open the audio sink at 48 kHz mono.
}

void AMReceiver::set_station_frequency(uint32_t station_hz) {
    const int64_t offset = 
        static_cast<int64_t>(m_config.center_frequency)
        - static_cast<int64_t>(station_hz);

    m_tuner.set_tuning_offset(static_cast<int32_t>(offset));
}

void AMReceiver::run() {
    m_running = true;

    while(m_running) {
        // 1. Read on block of complex IQ samples.
        size_t count = m_sdr.read_samples(m_buffer, 4096);

        if(count == 0)
            continue;

        // 2. Mix the selected station down to baseband.
        // tuner.process()


        // 3. Filter the complex baseband signal.
        // channel_filter.filter()

        // 4. AM envelope detection
        // Demodulator::am_demodulate()

        // 5. Remove DC and filter the audio.
        // for(sample) { dc_blocker.process(envelope) }
        // audio_filter.process()

        // 6. Properly resample to 48 kHz.
        // resampler.process()

        // 7. Apply volume and prevent clipping.
        // sampled_audio * config.audio_gain
        // sampled_audio = clamp(sampled_audio, -1, 1)

        // 8. Send the resulting mono samples to PipeWire.
        // audio.write(sampled_audio)
    }
}

void AMReceiver::stop() {
    m_running = false;
}