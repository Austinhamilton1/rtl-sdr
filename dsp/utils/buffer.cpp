#include "buffer.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
    constexpr double a = 0.75;
    constexpr double inverse_a = 0.25;

    size_t checked_capacity(size_t num_chunks) {
        if(num_chunks == 0) {
            throw std::invalid_argument(
                "Number of chunks must be greater than zero."
            );
        }

        if(num_chunks > detail::max_chunks) {
            throw std::length_error(
                "Number of chunks too large"
            );
        }

        size_t capacity = num_chunks * detail::chunk_size;

        return capacity;
    }
}

Buffer::Buffer(size_t num_chunks, uint64_t sample_rate) 
    : m_capacity_samples(checked_capacity(num_chunks)),
      m_sample_rate(sample_rate),
      m_buffer_I(m_capacity_samples), 
      m_buffer_Q(m_capacity_samples), 
      m_scratch_buffer_I(detail::chunk_size),
      m_scratch_buffer_Q(detail::chunk_size) {}

void Buffer::initialize(size_t num_chunks, uint64_t sample_rate) {
    m_capacity_samples = checked_capacity(num_chunks);
    m_sample_rate = sample_rate;
    m_buffer_I.resize(m_capacity_samples);
    m_buffer_Q.resize(m_capacity_samples);
    m_scratch_buffer_I.resize(detail::chunk_size);
    m_scratch_buffer_Q.resize(detail::chunk_size);
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

    m_buffer_I[index] = I;
    m_buffer_Q[index] = Q;
    
    m_head = head + 1; // Advance head after writing

    // Used by the heuristic only; this does not publish a completed chunk.
    m_write_position.store(m_head, std::memory_order_relaxed);

    // Publish only complete chunks. The DSP thread never sees a partially written chunk.
    if(m_head % detail::chunk_size == 0) {
        m_published_head.store(m_head, std::memory_order_release);
    }

    return 0; // Return success
}

void Buffer::register_dsp(DspCallback callback, uint64_t runtime_hint_ns) {
    if(callback == nullptr) {
        throw std::invalid_argument("Callback function cannot be null.");
    }

    m_dsp_callbacks.push_back({callback, runtime_hint_ns});
}

bool Buffer::has_complete_chunk(std::uint64_t tail, std::uint64_t published_head) const noexcept {
    return published_head >= tail &&
           published_head - tail >= detail::chunk_size;
}

bool Buffer::should_copy(std::uint64_t tail, std::uint64_t total_estimated_runtime_ns) const noexcept {
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

    const uint64_t required_ns =
        saturating_add(total_estimated_runtime_ns,
                       detail::safety_margin_ns);

    return static_cast<long double>(required_ns) >= available_ns;
}

void Buffer::invoke_callbacks(void *user_data, float *I, float *Q) {
    size_t output_shape = detail::chunk_size;

    for(auto& callback : m_dsp_callbacks) {
        auto start = std::chrono::steady_clock::now();
        output_shape = callback.func(user_data, I, Q, output_shape);
        auto end = std::chrono::steady_clock::now();

        auto Tn = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        double En =
            a * static_cast<double>(Tn)
            + inverse_a * static_cast<double>(callback.estimated_runtime_ns);

        callback.estimated_runtime_ns = static_cast<uint64_t>(En);
    }
}

uint64_t Buffer::total_estimated_runtime_ns() const noexcept {
    uint64_t total = 0;

    for(const auto& callback : m_dsp_callbacks) {
        total = saturating_add(total, callback.estimated_runtime_ns);
    }

    return total;
}

int Buffer::run_dsp_callbacks(void *user_data) {    
    // One consumer only
    while(true) {
        const uint64_t tail =
            m_tail.load(std::memory_order_relaxed);

        // Acquire pairs with the producer's release publication
        const uint64_t published_head =
            m_published_head.load(std::memory_order_acquire);

        if(!has_complete_chunk(tail, published_head)) {
            return 0;
        }

        const size_t sample_index =
            static_cast<size_t>(tail % m_capacity_samples);

        float *I = m_buffer_I.data() + sample_index;
        float *Q = m_buffer_Q.data() + sample_index;

        if(should_copy(tail, total_estimated_runtime_ns())) {
            // Copy the complete chunk before releasing its ring storage.
            std::memcpy(
                m_scratch_buffer_I.data(),
                I,
                detail::chunk_size * sizeof(float)
            );

            std::memcpy(
                m_scratch_buffer_Q.data(),
                Q,
                detail::chunk_size * sizeof(float)
            );

            // The producer may reuse the ring slot after this release.
            m_tail.store(tail + detail::chunk_size, std::memory_order_release);

            try {
                invoke_callbacks(user_data, m_scratch_buffer_I.data(), m_scratch_buffer_Q.data());
            } catch(...) {
                return -1;
            }
        } else {
            // Keep the ring slot owned until every callback has finished.
            try {
                invoke_callbacks(user_data, I, Q);
            } catch(...) {
                // Release the chunk even when a callback fails.
                m_tail.store(tail + detail::chunk_size, std::memory_order_release);

                return -1;
            }

            m_tail.store(tail + detail::chunk_size, std::memory_order_release);
        }
    }
}

uint64_t Buffer::get_execution_runtime_ns(DspCallback function) {
    for(const auto& callback : m_dsp_callbacks) {
        if(callback.func == function) {
            return callback.estimated_runtime_ns;
        }
    }

    return 0;
}