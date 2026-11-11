// SPDX-License-Identifier: GPL-3.0-or-later
//
// SampleSource: anything that produces raw cu8 IQ, on its own thread, into a
// block queue. Two exist:
//
//   RtlSdrSource  a stick, through librtlsdr
//   FileSource    a .cu8 recording with its JSON sidecar
//
// Everything downstream (DSP, recorder, UI) sees the same SampleBlocks and
// can't tell them apart, which is what lets M1–M5 be built and tested on
// recordings.

#pragma once

#include "dsp/SpscSlots.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace zarescope::device {

// Bytes per block: 32 768 complex samples, 13.7 ms at 2.4 MS/s. A multiple
// of 512, as librtlsdr's USB transfers require.
inline constexpr size_t kBlockBytes = 65'536;
// Queue depth: 128 blocks ≈ 1.7 s at 2.4 MS/s of slack for the DSP thread.
inline constexpr size_t kBlockQueueSlots = 128;

// One block of samples and what they are.
struct SampleBlock {
    std::vector<uint8_t> bytes;  // capacity kBlockBytes, reserved once; size = bytes used
    int64_t centerFrequencyHz = 0;
    uint32_t sampleRateSps = 0;
    std::chrono::system_clock::time_point firstSampleUtc;
    uint64_t sequence = 0;  // counts blocks produced, including dropped ones
};

using BlockQueue = dsp::SpscSlots<SampleBlock>;

// A queue whose slots each have kBlockBytes reserved.
inline BlockQueue makeBlockQueue(size_t slots = kBlockQueueSlots) {
    return BlockQueue(slots, [](SampleBlock& block) { block.bytes.reserve(kBlockBytes); });
}

enum class SourceKind { Device, File };

struct SourceInfo {
    SourceKind kind = SourceKind::File;
    std::string name;  // device name or file name
    int64_t centerFrequencyHz = 0;
    uint32_t sampleRateSps = 0;
};

class SampleSource {
public:
    virtual ~SampleSource() = default;
    SampleSource() = default;
    SampleSource(const SampleSource&) = delete;
    SampleSource& operator=(const SampleSource&) = delete;
    SampleSource(SampleSource&&) = delete;
    SampleSource& operator=(SampleSource&&) = delete;

    // Starts producing into `queue` on the source's own thread. On failure,
    // returns false and fills `error`. `queue` must outlive stop().
    virtual bool start(BlockQueue& queue, std::string& error) = 0;
    // Stops and joins the thread. Safe to call more than once.
    virtual void stop() = 0;

    // A snapshot of what the source is producing now. Thread-safe.
    [[nodiscard]] virtual SourceInfo info() const = 0;

    // True once the source has stopped by itself: the file ended (when not
    // looping) or the stick was unplugged. lastError() says why, if it failed.
    [[nodiscard]] bool finished() const { return m_finished.load(std::memory_order_acquire); }
    [[nodiscard]] std::string lastError() const;
    // Blocks thrown away because the queue was full (the consumer fell behind).
    [[nodiscard]] uint64_t droppedBlocks() const {
        return m_dropped.load(std::memory_order_relaxed);
    }

protected:
    // Copies `bytes` into the next free slot and stamps it. Called on the
    // producer thread only. Returns false (and counts a drop) if the queue
    // is full. Never allocates: `bytes` must fit in kBlockBytes.
    bool pushBlock(BlockQueue& queue, std::span<const uint8_t> bytes, int64_t centerFrequencyHz,
                   uint32_t sampleRateSps, std::chrono::system_clock::time_point firstSampleUtc);

    void finish(std::string error = {});
    void resetState();

private:
    std::atomic<bool> m_finished{false};
    std::atomic<uint64_t> m_dropped{0};
    uint64_t m_sequence = 0;  // producer thread only
    mutable std::atomic<bool> m_errorReady{false};
    std::string m_error;  // written once before m_errorReady is set
};

}  // namespace zarescope::device
