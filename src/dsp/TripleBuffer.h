// SPDX-License-Identifier: GPL-3.0-or-later
//
// TripleBuffer: hands the latest value from one writer thread to one reader
// thread, lock-free, without either ever waiting.
//
// Three copies of T: the writer fills its own copy, then publishes it by
// swapping it with the shared middle copy; the reader swaps the middle copy
// with its own when a newer one is there. The reader always gets a complete
// value (never half-written), and skips values it was too slow to see:
// right for "the current spectrum trace", wrong for "every waterfall row"
// (that's SpscSlots).

#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace zarescope::dsp {

template <typename T>
class TripleBuffer {
public:
    // `init` prepares each of the three copies (e.g. reserves a vector).
    template <typename Init>
    explicit TripleBuffer(Init&& init) {
        for (T& value : m_buffers) {
            init(value);
        }
    }
    TripleBuffer() : TripleBuffer([](T&) {}) {}

    // Writer: the copy to fill. Stays the writer's until publish().
    T& writeBuffer() { return m_buffers[m_writeIndex]; }
    // Writer: makes the filled copy the latest.
    void publish() {
        const uint8_t old = m_middle.exchange(static_cast<uint8_t>(m_writeIndex | kFresh),
                                              std::memory_order_acq_rel);
        m_writeIndex = old & kIndexMask;
    }

    // Reader: true (and swaps it in) when a value newer than readBuffer()
    // has been published.
    bool update() {
        if ((m_middle.load(std::memory_order_relaxed) & kFresh) == 0) {
            return false;
        }
        const uint8_t old =
            m_middle.exchange(static_cast<uint8_t>(m_readIndex), std::memory_order_acq_rel);
        m_readIndex = old & kIndexMask;
        return true;
    }
    // Reader: the most recent value update() brought in.
    const T& readBuffer() const { return m_buffers[m_readIndex]; }

private:
    static constexpr uint8_t kIndexMask = 0x3;
    static constexpr uint8_t kFresh = 0x4;  // the middle copy hasn't been read yet

    std::array<T, 3> m_buffers{};
    uint8_t m_writeIndex = 0;          // writer only
    uint8_t m_readIndex = 1;           // reader only
    std::atomic<uint8_t> m_middle{2};  // shared: index | kFresh
};

}  // namespace zarescope::dsp
