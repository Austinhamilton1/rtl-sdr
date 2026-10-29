#include "radio.h"

#include <iostream>
#include <stdexcept>

void SDRRadio::open() {
    int result = rtlsdr_open(&m_dev, 0);

    if(result < 0) {
        throw std::runtime_error("Could not open RTL-SDR device");
    }

    m_raw.resize(
        static_cast<size_t>(detail::chunk_size) * 2
    );
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

void SDRRadio::reset_buffer() {
    if(rtlsdr_reset_buffer(m_dev) < 0) {
        close();
        throw std::runtime_error("Could not reset buffer");
    }
}

void SDRRadio::set_direct_sampling(int mode) {
    if(mode < 0 || mode > 2) {
        throw std::invalid_argument(
            "Direct sampling mode must be 0, 1, or 2"
        );
    }

    if(rtlsdr_set_direct_sampling(m_dev, mode) < 0) {
        throw std::runtime_error(
            "Could not set direct sampling mode."
        );
    }
}

int64_t SDRRadio::read_samples(Buffer& buffer) {
    int bytes_read = 0;

    const int result = rtlsdr_read_sync(
        m_dev,
        m_raw.data(),
        static_cast<int>(m_raw.size()),
        &bytes_read
    );

    if (result < 0) {
        std::cerr << "rtlsdr_read_sync failed: "
                  << result << '\n';
        return -1;
    }

    if (bytes_read < 0 ||
        bytes_read > static_cast<int>(m_raw.size()) ||
        bytes_read % 2 != 0) {
        std::cerr << "Invalid byte count: "
                  << bytes_read << '\n';
        return -1;
    }

    const size_t complex_samples =
        static_cast<size_t>(bytes_read) / 2;

    for (size_t n = 0; n < complex_samples; ++n) {
        const float i_sample =
            (static_cast<float>(m_raw[2 * n]) - 127.5f) / 127.5f;

        const float q_sample =
            (static_cast<float>(m_raw[2 * n + 1]) - 127.5f) / 127.5f;

        if (buffer.push(i_sample, q_sample) < 0) {
            std::cerr << "Buffer push failed at sample "
                      << n << " of " << complex_samples << '\n';
            return -1;
        }
    }

    return static_cast<int64_t>(complex_samples);
}

void SDRRadio::close() {
    rtlsdr_close(m_dev);
}