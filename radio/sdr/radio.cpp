#include "radio.h"

#include <stdexcept>

void SDRRadio::open() {
    int result = rtlsdr_open(&m_dev, 0);

    if(result < 0) {
        throw std::runtime_error("Could not open RTL-SDR device");
    }
}

void SDRRadio::set_center_frequency(uint32_t frequency) {
    if(rtlsdr_set_center_freq(m_dev, frequency) < 0) {
        close();
        throw std::runtime_error("Could not set center frequency");
    }
}

uint32_t SDRRadio::get_center_frequency() {
    return rtlsdr_get_center_freq(m_dev);
}

void SDRRadio::set_sample_rate(uint64_t samples_per_second) {
    if(rtlsdr_set_sample_rate(m_dev, samples_per_second) < 0) {
        close();
        throw std::runtime_error("Could not set sample rate");
    }
}

void SDRRadio::set_rx_bandwidth(uint32_t hz) {
    if(rtlsdr_set_tuner_bandwidth(m_dev, hz) < 0) {
        close();
        throw std::runtime_error("Could not set receive bandwidth");
    }
}

void SDRRadio::set_gain_mode(bool automatic) {
    if(automatic) {
        if(rtlsdr_set_tuner_gain_mode(m_dev, 0) < 0) {
            close();
            throw std::runtime_error("Could not set gain mode to automatic");
        }
    } else {
        if(rtlsdr_set_tuner_gain_mode(m_dev, 1) < 0) {
            close();
            throw std::runtime_error("Could not set gain mode to manual");
        }
    }
}

void SDRRadio::set_gain(int gain) {
    if(rtlsdr_set_tuner_gain(m_dev, gain) < 0) {
        close();
        throw std::runtime_error("Could not set gain");
    }
}

int64_t SDRRadio::read_samples(Buffer& buffer, int32_t num_samples) {
    int values_read = 0;
    int16_t *buf = new int16_t[num_samples * 2]; // Each sample has I and Q components

    int result = rtlsdr_read_sync(m_dev, buf, num_samples * sizeof(int16_t) * 2, &values_read);

    if(result < 0) {
        delete[] buf;
        return -1; // Return -1 to indicate failure
    }

    for(int i = 0; i < values_read / 2; ++i) {
        const int16_t i_sample = buf[2 * i];
        const int16_t q_sample = buf[2 * i + 1];
        buffer.push(static_cast<float>(i_sample) / 32768.0f, static_cast<float>(q_sample) / 32768.0f);
    }

    delete[] buf;
    return values_read / 2;
}

void SDRRadio::close() {
    rtlsdr_close(m_dev);
}