#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace detail {
    constexpr uint64_t safety_margin_ns = 1000000; // 1 ms safety margin
    constexpr size_t chunk_size = 4096; // Size of each chunk in the buffer
}

class Buffer {
    using DspCallback = void (*)(float *interleaved_iq, size_t complex_samples);

public:
    /*
     * Constructor for the Buffer class.
     * @param num_chunks The number of chunks in the buffer.
     * @param sample_rate The sample rate of the buffer.
     */
    explicit Buffer(size_t num_chunks, uint64_t sample_rate);
    
    ~Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    /*
     * Pushes a pair of float values (I and Q) into the buffer.
     * @param I The in-phase component to be pushed into the buffer.
     * @param Q The quadrature component to be pushed into the buffer.
     * @return An integer indicating the success or failure of the push operation.
     */
    int push(float I, float Q);

    /*
     * Registers a DSP function to be called when a chunk of the buffer is complete.
     * @param callback The DSP function to be registered.
     */
    void register_dsp(DspCallback callback, uint64_t complete_ns_hint);

    int run_dsp_callbacks();

private:
    static constexpr uint64_t saturating_add(uint64_t a, uint64_t b) noexcept {
        constexpr auto max = std::numeric_limits<uint64_t>::max();
        return b > max - a ? max : a + b;
    }

    bool has_complete_chunk(uint64_t tail, uint64_t published_head) const noexcept;

    bool should_copy(uint64_t tail, uint64_t total_callback_hint_ns) const noexcept;

    void invoke_callback(float *data);

    const size_t m_capacity_samples;
    const uint64_t m_sample_rate;

    // Fixed allocations: no allocation in push() or normal DSP path;
    std::vector<float> m_buffer;
    std::vector<float> m_scratch_buffer;

    // Producer-owned write position, in complex samples (I/Q pairs)
    uint64_t m_head{0};

    // Published producer position, advanced only at chunk boundaries.
    std::atomic<uint64_t> m_published_head{0};

    // Current producer positoin, used only for the runtime heuristic
    std::atomic<uint64_t> m_write_position{0};
    
    // Consumer's released position, in complex samples (I/Q pairs)
    std::atomic<uint64_t> m_tail{0};

    std::vector<std::pair<DspCallback, uint64_t>> m_dsp_callbacks;
    uint64_t m_total_callback_hint_ns{0};
};