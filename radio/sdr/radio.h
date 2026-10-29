#pragma once

#include <cstddef>
#include <cstdint>
#include <rtl-sdr.h>
#include <vector>

#include "dsp/utils/buffer.h"

class SDRRadio {
public:
    /*
     * Opens the SDR radio. This function must be called before any other operations on the radio.
     */
    void open();

    /*
     * Sets the center frequency of the SDR radio.
     * @param frequency The center frequency in Hz.
     */
    void set_center_frequency(uint32_t frequency);

    /*
     * Gets the current center frequency of the SDR radio.
     * @return The center frequency in Hz.
     */
    uint32_t get_center_frequency();

    /*
     * Sets the sample rate of the SDR radio.
     * @param samples_per_second The sample rate in samples per second.
     */
    void set_sample_rate(uint64_t samples_per_second);

    /*
     * Sets the receive bandwidth of the SDR radio.
     * @param hz The receive bandwidth in Hz.
     */
    void set_rx_bandwidth(uint32_t hz);

    /*
     * Sets the gain mode of the SDR radio.
     * @param automatic The gain mode (true for automatic, false for manual).
     */
    void set_gain_mode(bool automatic);

    /*
     * Sets the gain of the SDR radio.
     * @param gain The gain value.
     */
    void set_gain(int gain);

    /*
     * Reset the SDR buffer.
     */
    void reset_buffer();

    /*
     * Set the direct sampling mode.
     */
    void set_direct_sampling(int mode);

    /*
     * Reads samples from the SDR radio.
     * @param buffer The buffer to read samples into.
     * @return The number of samples actually read or -1 on failure.
     */
    int64_t read_samples(Buffer& buffer);

    /*
     * Closes the SDR radio.
     */
    void close();

private:
    rtlsdr_dev_t *m_dev = nullptr;
    std::vector<uint8_t> m_raw;
};