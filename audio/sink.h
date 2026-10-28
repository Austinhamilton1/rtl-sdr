#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include <pipewire-0.3/pipewire/pipewire.h>
#include <pipewire-0.3/pipewire/main-loop.h>
#include <pipewire-0.3/pipewire/stream.h>

class AudioSink {
public:
    explicit AudioSink(uint32_t sample_rate = 48000, size_t buffer_capacity = 96000);
    ~AudioSink();

    AudioSink(const AudioSink&) = delete;
    AudioSink& operator=(const AudioSink&) = delete;

    void initialize(uint32_t sample_rate = 48000, size_t buffer_capacity = 96000);

    void open();
    void close() noexcept;

    size_t write(const float *samples, size_t count);

    uint32_t sample_rate() const noexcept {
        return m_sample_rate;
    }

    uint64_t underruns() const noexcept {
        return m_underruns.load(std::memory_order_relaxed);
    }

private:
    size_t read_samples(float *output, size_t count) noexcept;

    static void on_process(void *userdata);
    static void on_state_changed(
        void *userdata,
        pw_stream_state old_state,
        pw_stream_state state,
        const char *error
    );

    void process() noexcept;

    uint32_t m_sample_rate;
    std::vector<float> m_ring;

    // Monotonically increasing sample positions.
    std::atomic<size_t> m_write_index{0};
    std::atomic<size_t> m_read_index{0};

    pw_main_loop *m_loop = nullptr;
    pw_stream *m_stream = nullptr;
    pw_stream_events m_events{};

    std::thread m_loop_thread;
    std::atomic<bool> m_open{false};

    std::atomic<uint64_t> m_underruns{0};
};