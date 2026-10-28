#include "sink.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <pipewire-0.3/pipewire/keys.h>
#include <pipewire-0.3/pipewire/port.h>
#include <pipewire-0.3/pipewire/properties.h>

#include <spa-0.2/spa/buffer/buffer.h>
#include <spa-0.2/spa/param/format-utils.h>
#include <spa-0.2/spa/param/param.h>
#include <spa-0.2/spa/param/audio/raw.h>
#include <spa-0.2/spa/param/audio/raw-utils.h>
#include <spa-0.2/spa/pod/builder.h>
#include <spa-0.2/spa/utils/defs.h>

#include <pipewire-0.3/pipewire/core.h>

AudioSink::AudioSink(uint32_t sample_rate, size_t buffer_capacity)
    : m_sample_rate(sample_rate),
      m_ring(buffer_capacity)
{
    if(sample_rate == 0) {
        throw std::invalid_argument("Sample rate must be positive");
    }

    if(buffer_capacity == 0) {
        throw std::invalid_argument("Buffer capacity must be positive");
    }
}

AudioSink::~AudioSink() {
    close();
}

void AudioSink::initialize(uint32_t sample_rate, size_t buffer_capacity) {
    if(sample_rate == 0) {
        throw std::invalid_argument("Sample rate must be positive");
    }

    if(buffer_capacity == 0) {
        throw std::invalid_argument("Buffer capacity must be positive");
    }

    m_sample_rate = sample_rate;
    m_ring.resize(buffer_capacity);
}

void AudioSink::open() {
    if(m_open.load())
        return;

    pw_init(nullptr, nullptr);

    m_loop = pw_main_loop_new(nullptr);
    if(!m_loop) {
        pw_deinit();
        throw std::runtime_error("Failed to create PipeWire main loop");
    }

    pw_properties *properties = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "AM Radio",
        nullptr
    );

    if(!properties) {
        pw_main_loop_destroy(m_loop);
        m_loop = nullptr;
        pw_deinit();
        throw std::runtime_error("Failed to create PipeWire properties");
    }

    m_events.version = PW_VERSION_STREAM_EVENTS;
    m_events.state_changed = &AudioSink::on_state_changed;
    m_events.process = &AudioSink::on_process;

    m_stream = pw_stream_new_simple(
        pw_main_loop_get_loop(m_loop),
        "am-radio-playback",
        properties,
        &m_events,
        this
    );

    // pw_stream_new_simple takes ownership of the properties.
    if(!m_stream) {
        pw_main_loop_destroy(m_loop);
        m_loop = nullptr;
        pw_deinit();
        throw std::runtime_error("Failed to create PipeWire stream.");
    }

    // Request mono, interleaved 32-bit floating-point audio.
    spa_audio_info_raw audio_info{};
    audio_info.format = SPA_AUDIO_FORMAT_F32;
    audio_info.channels = 1;
    audio_info.rate = m_sample_rate;

    uint8_t pod_buffer[1024];
    spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));

    const spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(
        &builder, SPA_PARAM_EnumFormat, &audio_info
    );

    int result = pw_stream_connect(
        m_stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(
            PW_STREAM_FLAG_AUTOCONNECT |
            PW_STREAM_FLAG_MAP_BUFFERS |
            PW_STREAM_FLAG_RT_PROCESS
        ),
        params,
        1
    );

    if(result < 0) {
        pw_stream_destroy(m_stream);
        m_stream = nullptr;
        pw_main_loop_destroy(m_loop);
        m_loop = nullptr;
        pw_deinit();
        throw std::runtime_error("Failed to connect PipeWire stream");
    }

    m_open.store(true);

    try {
        m_loop_thread = std::thread([this] {
            pw_main_loop_run(m_loop);
        });
    } catch(...) {
        m_open.store(false);
        pw_stream_disconnect(m_stream);
        pw_stream_destroy(m_stream);
        m_stream = nullptr;
        pw_main_loop_destroy(m_loop);
        m_loop = nullptr;
        pw_deinit();
        throw;
    }
}

void AudioSink::close() noexcept {
    if(!m_loop) return;

    m_open.store(false);

    pw_main_loop_quit(m_loop);

    if(m_loop_thread.joinable())
        m_loop_thread.join();

    if(m_stream) {
        pw_stream_disconnect(m_stream);
        pw_stream_destroy(m_stream);
        m_stream = nullptr;
    }

    pw_main_loop_destroy(m_loop);
    m_loop = nullptr;

    pw_deinit();
}

size_t AudioSink::write(const float *samples, size_t count) {
    if(count == 0)
        return 0;

    if(!samples)
        throw std::invalid_argument("Audio samples cannot be null");

    // Single producer: only the SDR/DSP thread calls write().
    const size_t write_pos = m_write_index.load(
        std::memory_order_relaxed
    );

    const size_t read_pos = m_read_index.load(
        std::memory_order_acquire
    );

    const size_t used = write_pos - read_pos;
    const size_t available = m_ring.size() - used;
    const size_t accepted = std::min(count, available);

    for(size_t i = 0; i < accepted; i++) {
        m_ring[(write_pos + i) % m_ring.size()] = samples[i];
    }

    // Publish the newly written samples only after copying them.
    m_write_index.store(
        write_pos + accepted,
        std::memory_order_release
    );

    return accepted;
}

size_t AudioSink::read_samples(float *output, size_t count) noexcept {
    // Single consumer: only PipeWire's process callback calls this.
    const size_t read_pos = m_read_index.load(
        std::memory_order_relaxed
    );

    const size_t write_pos = m_write_index.load(
        std::memory_order_acquire
    );

    const size_t available = write_pos - read_pos;
    const size_t consumed = std::min(count, available);

    for(size_t i = 0; i < consumed; i++) {
        output[i] = m_ring[(read_pos + i) % m_ring.size()];
    }

    // Release the consumed slots to the producer.
    m_read_index.store(
        read_pos + consumed,
        std::memory_order_release
    );

    return consumed;
}

void AudioSink::on_process(void *userdata) {
    auto *self = static_cast<AudioSink *>(userdata);
    self->process();
}

void AudioSink::on_state_changed(
    void *userdata,
    pw_stream_state old_state,
    pw_stream_state state,
    const char *error
) {
    (void)userdata;
    (void)old_state;

    if(state == PW_STREAM_STATE_ERROR) {
        std::cerr << "PipeWire stream error: "
                  << (error ? error : "unknown error")
                  << std::endl;
    }
}

void AudioSink::process() noexcept {
    pw_buffer *pw_buf = pw_stream_dequeue_buffer(m_stream);
    if(!pw_buf)
        return;

    spa_buffer *buffer = pw_buf->buffer;

    if(buffer->n_datas > 0) {
        spa_data *data = &buffer->datas[0];

        if(data->data && data->chunk) {
            auto *output = static_cast<float *>(data->data);

            const size_t max_frames =
                data->maxsize / sizeof(float);

            size_t frames = max_frames;

            if(pw_buf->requested > 0) {
                frames = std::min(
                    frames,
                    static_cast<size_t>(pw_buf->requested)
                );
            }

            const size_t copied = read_samples(output, frames);

            if(copied < frames) {
                m_underruns.fetch_add(1, std::memory_order_relaxed);

                // Silence on underrun, rather than clocking playback.
                std::fill(
                    output + copied,
                    output + frames,
                    0.0f
                );
            }

            data->chunk->offset = 0;
            data->chunk->stride = sizeof(float);
            data->chunk->size =
                static_cast<uint32_t>(frames * sizeof(float));
        }
    }

    pw_stream_queue_buffer(m_stream, pw_buf);
}