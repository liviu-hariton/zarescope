// SPDX-License-Identifier: GPL-3.0-or-later
//
// IqRecorder: writes raw cu8 blocks to a .cu8 file plus its JSON sidecar.
//
// Three threads are involved, and none of them waits on another:
//   UI thread      start() / requestStop() / status()
//   DSP thread     offer(block) for every block: a copy into the recorder's
//                  own queue, or nothing when not recording
//   writer thread  owned by the recorder: creates the folder and files,
//                  writes blocks, and at the end rewrites the sidecar with
//                  the sample count
// So the disk never slows down the UI or the DSP chain.
//
// A recording holds one frequency and one sample rate. If a block arrives
// with a different one (the user retuned), the recording ends there and
// status().endedByRetune says so. Blocks still queued from before a retune
// that happened just before start() are skipped instead: the recording
// begins with the first block at its own frequency and rate.

#pragma once

#include "device/IqMetadata.h"
#include "device/SampleSource.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace zarescope::device {

enum class RecorderState { Idle, Recording, Finishing };

struct RecorderStatus {
    RecorderState state = RecorderState::Idle;
    std::filesystem::path path;  // the .cu8 file of the current or last recording
    std::chrono::system_clock::time_point startUtc;
    uint64_t bytesWritten = 0;
    uint64_t droppedBlocks = 0;  // the disk fell behind
    bool endedByRetune = false;
    std::string error;  // empty unless the last recording failed
};

class IqRecorder {
public:
    IqRecorder();
    ~IqRecorder();
    IqRecorder(const IqRecorder&) = delete;
    IqRecorder& operator=(const IqRecorder&) = delete;
    IqRecorder(IqRecorder&&) = delete;
    IqRecorder& operator=(IqRecorder&&) = delete;

    // UI thread. Starts a recording in `folder` (created if needed) described
    // by `metadata` (frequency, rate, gain, device; startUtc names the file).
    // Returns false while a previous recording is still finishing.
    // `expectedBytes`, when known (a satellite pass of known length), is
    // checked against the free disk space first: the recording fails at once
    // with a clear error rather than filling the disk halfway through.
    bool start(std::filesystem::path folder, IqMetadata metadata, uint64_t expectedBytes = 0);

    // UI thread. Ends the recording; the writer flushes and finishes the
    // sidecar in the background. Returns at once.
    void requestStop();

    // DSP thread. Never blocks, never allocates.
    void offer(const SampleBlock& block);

    // UI thread (any thread, really): a snapshot.
    [[nodiscard]] RecorderStatus status() const;

    // Blocks until the writer thread has finished. For tests and shutdown.
    void waitUntilIdle();

private:
    void run(std::filesystem::path folder, IqMetadata metadata, uint64_t expectedBytes);

    // How many mismatched blocks may be skipped before the first matching
    // one: the whole queue's worth, i.e. everything produced before start().
    static constexpr int kMaxSkippedAtStart = static_cast<int>(kBlockQueueSlots);

    BlockQueue m_queue;
    std::atomic<bool> m_recording{false};
    std::atomic<int> m_offersInFlight{0};
    std::atomic<int64_t> m_centerFrequencyHz{0};
    std::atomic<uint32_t> m_sampleRateSps{0};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_retuned{false};
    // DSP thread only (reset by start() before the gate opens).
    bool m_matched = false;
    int m_skipped = 0;
    std::atomic<uint64_t> m_bytesWritten{0};
    std::atomic<uint64_t> m_dropped{0};

    mutable std::mutex m_statusMutex;  // guards m_status's path/start/error/state
    RecorderStatus m_status;
    std::thread m_writer;
};

}  // namespace zarescope::device
