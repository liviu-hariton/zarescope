// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/AudioOutput.h"

#include <miniaudio.h>

namespace zarescope::audio {

namespace {

// The ring holds up to a second at 192 kHz; latency is held far below that.
constexpr size_t kRingSamples = 192'000;
// How much audio may wait in the ring before the oldest is skipped: 150 ms.
constexpr double kMaxLatencySeconds = 0.15;

}  // namespace

struct AudioOutput::Device {
    ma_device device{};
    bool initialised = false;
    size_t maxLatency = 0;
};

AudioOutput::AudioOutput() : m_device(std::make_unique<Device>()), m_ring(kRingSamples) {}

AudioOutput::~AudioOutput() { stop(); }

bool AudioOutput::start(std::string& error) {
    if (m_device->initialised) {
        return true;
    }
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate = 0;  // the device's native rate
    config.pUserData = this;
    config.dataCallback = [](ma_device* device, void* output, const void* /*input*/,
                             ma_uint32 frames) {
        auto* self = static_cast<AudioOutput*>(device->pUserData);
        self->m_ring.read(std::span<float>(static_cast<float*>(output), frames),
                          self->m_device->maxLatency);
    };
    if (ma_device_init(nullptr, &config, &m_device->device) != MA_SUCCESS) {
        error = "no sound output device is available";
        return false;
    }
    m_device->initialised = true;
    m_device->maxLatency = static_cast<size_t>(kMaxLatencySeconds * m_device->device.sampleRate);
    if (ma_device_start(&m_device->device) != MA_SUCCESS) {
        error = "the sound output device could not start";
        stop();
        return false;
    }
    return true;
}

void AudioOutput::stop() {
    if (m_device->initialised) {
        ma_device_uninit(&m_device->device);
        m_device->initialised = false;
    }
}

bool AudioOutput::running() const {
    return m_device->initialised && ma_device_is_started(&m_device->device) == MA_TRUE;
}

uint32_t AudioOutput::sampleRate() const {
    return m_device->initialised ? m_device->device.sampleRate : 0;
}

}  // namespace zarescope::audio
