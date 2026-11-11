// SPDX-License-Identifier: GPL-3.0-or-later
//
// AudioOutput: the default sound output device, mono, 32-bit float, at the
// device's own sample rate, through miniaudio (portable: CoreAudio on
// macOS, the system's API elsewhere).
//
// The device pulls samples on its own real-time thread from an AudioRing,
// which the DSP thread fills. start() and stop() talk to the audio system
// and can take a moment: call them off the UI thread (the Receiver's control
// thread does).

#pragma once

#include "audio/AudioRing.h"

#include <cstdint>
#include <memory>
#include <string>

namespace zarescope::audio {

class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    AudioOutput(AudioOutput&&) = delete;
    AudioOutput& operator=(AudioOutput&&) = delete;

    // Opens and starts the default output. False (with `error`) if there is
    // no usable device.
    bool start(std::string& error);
    void stop();

    [[nodiscard]] bool running() const;
    // The device's sample rate (valid after start()).
    [[nodiscard]] uint32_t sampleRate() const;
    [[nodiscard]] AudioRing& ring() { return m_ring; }

private:
    struct Device;
    std::unique_ptr<Device> m_device;
    AudioRing m_ring;
};

}  // namespace zarescope::audio
