#include "buffer.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

Buffer::Buffer(size_t num_chunks, uint64_t sample_rate) 
    : m_capacity_samples(num_chunks * detail::chunk_size),
      m_sample_rate(sample_rate),
      m_buffer(m_capacity_samples * 2), // Allocate space for I/Q pairs
      m_scratch_buffer(detail::chunk_size * 2)
{
    if (num_chunks == 0) {
        throw std::invalid_argument("Number of chunks must be greater than zero.");
    }

    if (sample_rate == 0) {
        throw std::invalid_argument("Sample rate must be greater than zero.");
    }

    // Keep absolute positions and capacity arithmetic representable.
    if(num_chunks > std::numeric_limits<uint64_t>::max() / detail::chunk_size * 2) {
        throw std::invalid_argument("Number of chunks is too large.");
    }
}

int Buffer::push(float I, float Q) {
    // Single producer: this position is owned by the producer thread.
    const uint64_t head = m_head;

    const uint64_t tail = m_tail.load(std::memory_order_acquire);

    if(head - tail >= m_capacity_samples) {
        return -1; // Buffer is full
    }

    const size_t index = 
        static_cast<size_t>(head % m_capacity_samples);

    m_buffer[index * 2] = I;
    m_buffer[index * 2 + 1] = Q;
    
    m_head = head + 1; // Advance head after writing

    // Used by the heuristic only; this does not publish a completed chunk.
    m_write_position.store(m_head, std::memory_order_relaxed);

    // Publish only complete chunks. The DSP thread never sees a partially written chunk.
    if(m_head % detail::chunk_size == 0) {
        m_published_head.store(m_head, std::memory_order_release);
    }

    return 0; // Return success
}

void Buffer::register_dsp(DspCallback callback, uint64_t complete_ns_hint) {
    if(callback == nullptr) {
        throw std::invalid_argument("Callback function cannot be null.");
    }

    m_dsp_callbacks.push_back({callback, complete_ns_hint});

    m_total_callback_hint_ns = 
        saturating_add(m_total_callback_hint_ns, complete_ns_hint);
}

bool Buffer::has_complete_chunk(std::uint64_t tail, std::uint64_t published_head) const noexcept {
    return published_head >= tail &&
           published_head - tail >= detail::chunk_size;
}

bool Buffer::should_copy(std::uint64_t tail, std::uint64_t total_callback_hint_ns) const noexcept {
    const uint64_t write_position =
        m_write_position.load(std::memory_order_relaxed);

    // The producer must never be more than capacity samples ahead.
    const uint64_t used = write_position - tail;

    if (used >= m_capacity_samples) {
        return true;
    }

    const uint64_t free_samples = m_capacity_samples - used;

    // Calculate in floating point to avoid integer truncation to seconds.
    const long double available_ns =
        static_cast<long double>(free_samples) * 1'000'000'000.0L /
        static_cast<long double>(m_sample_rate);

    const std::uint64_t required_ns =
        saturating_add(total_callback_hint_ns,
                       detail::safety_margin_ns);

    return static_cast<long double>(required_ns) >= available_ns;
}

void Buffer::invoke_callback(float *data) {
    for(const auto& [callback, hint] : m_dsp_callbacks) {
        (void)hint;
        callback(data, detail::chunk_size);
    }
}

int Buffer::run_dsp_callbacks() {    
    const uint64_t tail = m_tail.load(std::memory_order_acquire);
    const uint64_t head = m_head.load(std::memory_order_acquire);
    const uint64_t time_until_reuse = (m_size - (head - tail) / 2) / (m_sample_rate);

    for(const auto &callback : m_dsp_callbacks) {
        if(callback.complete_ns_hint + detail::safety_margin_ns <= time_until_reuse) {
            try {
                callback.func(m_buffer + (tail & m_mask) * 2);
            } catch(...) {
                return -1; // Return error if callback fails
            }

            m_tail.fetch_add(m_size * 2, std::memory_order_release); // Move tail forward after processing
        } else {
            float *scratch_buffer = new float[m_size * 2];
            std::memcpy(scratch_buffer, m_buffer + (tail & m_mask) * 2, m_size * 2 * sizeof(float));
            m_tail.fetch_add(m_size * 2, std::memory_order_release); // Move tail forward after copying

            try {
                callback.func(scratch_buffer);
            } catch(...) {
                delete[] scratch_buffer; // Clean up the scratch buffer
                return -1; // Return error if callback fails
            } 
            
            delete[] scratch_buffer; // Clean up the scratch buffer
        }
    }
    return 0; // Return success
}