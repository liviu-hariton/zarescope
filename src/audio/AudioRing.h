// SPDX-License-Identifier: GPL-3.0-or-later
//
// AudioRing: a lock-free single-producer/single-consumer ring of audio
// samples, between the DSP thread (writes) and the sound card's callback
// (reads). Neither side ever waits or allocates.
//
// The two clocks (the stick's and the sound card's) never agree exactly, so
// the fill level drifts. The reader keeps latency bounded: when more than
// `maxLatency` samples are waiting, it skips the oldest (a tiny, inaudible
// jump); when too few are there, it plays silence for the gap.

#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <span>
#include <vector>

namespace zarescope::audio {

class AudioRing {
public:
    explicit AudioRing(size_t capacity)
        : m_buffer(std::bit_ceil(capacity < 2 ? 2 : capacity)), m_mask(m_buffer.size() - 1) {}

    // Producer: writes as much of `samples` as fits; returns how much.
    size_t write(std::span<const float> samples) {
        const size_t head = m_head.load(std::memory_order_relaxed);
        const size_t tail = m_tail.load(std::memory_order_acquire);
        const size_t room = m_buffer.size() - (head - tail);
        const size_t n = samples.size() < room ? samples.size() : room;
        for (size_t i = 0; i < n; ++i) {
            m_buffer[(head + i) & m_mask] = samples[i];
        }
        m_head.store(head + n, std::memory_order_release);
        return n;
    }

    // Consumer: fills `out` with the oldest samples, silence for any
    // shortfall. Skips ahead first when more than `maxLatency` are waiting.
    // Returns how many real samples were read.
    size_t read(std::span<float> out, size_t maxLatency) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        const size_t head = m_head.load(std::memory_order_acquire);
        if (head - tail > maxLatency + out.size()) {
            tail = head - maxLatency;  // drop the oldest: keep latency bounded
            m_skips.fetch_add(1, std::memory_order_relaxed);
        }
        const size_t available = head - tail;
        const size_t n = out.size() < available ? out.size() : available;
        for (size_t i = 0; i < n; ++i) {
            out[i] = m_buffer[(tail + i) & m_mask];
        }
        for (size_t i = n; i < out.size(); ++i) {
            out[i] = 0.0F;
        }
        if (n < out.size()) {
            m_underruns.fetch_add(1, std::memory_order_relaxed);
        }
        m_tail.store(tail + n, std::memory_order_release);
        return n;
    }

    [[nodiscard]] size_t capacity() const { return m_buffer.size(); }
    [[nodiscard]] size_t sizeApprox() const {
        return m_head.load(std::memory_order_acquire) - m_tail.load(std::memory_order_acquire);
    }
    [[nodiscard]] uint64_t underruns() const { return m_underruns.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t skips() const { return m_skips.load(std::memory_order_relaxed); }

private:
    static constexpr size_t kCacheLine = 64;
    std::vector<float> m_buffer;
    size_t m_mask;
    alignas(kCacheLine) std::atomic<size_t> m_head{0};
    alignas(kCacheLine) std::atomic<size_t> m_tail{0};
    std::atomic<uint64_t> m_underruns{0};
    std::atomic<uint64_t> m_skips{0};
};

}  // namespace zarescope::audio
