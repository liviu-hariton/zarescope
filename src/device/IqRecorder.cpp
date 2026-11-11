// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/IqRecorder.h"

#include <fstream>

namespace zarescope::device {

IqRecorder::IqRecorder() : m_queue(makeBlockQueue()) {}

IqRecorder::~IqRecorder() {
    requestStop();
    waitUntilIdle();
}

bool IqRecorder::start(std::filesystem::path folder, IqMetadata metadata) {
    {
        const std::scoped_lock lock(m_statusMutex);
        if (m_status.state != RecorderState::Idle) {
            return false;
        }
    }
    if (m_writer.joinable()) {
        m_writer.join();  // the previous recording's thread has already ended
    }
    // The queue is empty here: the previous writer drained it after the last
    // offer() finished (see run()).
    m_stopRequested.store(false);
    m_retuned.store(false);
    m_bytesWritten.store(0);
    m_dropped.store(0);
    m_centerFrequencyHz.store(metadata.centerFrequencyHz);
    m_sampleRateSps.store(metadata.sampleRateSps);
    {
        const std::scoped_lock lock(m_statusMutex);
        m_status = RecorderStatus{};
        m_status.state = RecorderState::Recording;
        m_status.path = folder / recordingFileName(metadata.startUtc, metadata.centerFrequencyHz);
        m_status.startUtc = metadata.startUtc;
    }
    m_writer = std::thread([this, folder = std::move(folder), metadata = std::move(metadata)] {
        run(folder, metadata);
    });
    // Open the gate last, so offer() only ever sees a ready writer.
    m_recording.store(true);
    return true;
}

void IqRecorder::requestStop() {
    // Sequentially consistent (the default) on purpose: offer() increments
    // m_offersInFlight and then reads m_recording, while the writer clears
    // m_recording and then reads m_offersInFlight. Only seq_cst rules out
    // both sides missing each other.
    m_recording.store(false);
    m_stopRequested.store(true);
}

void IqRecorder::offer(const SampleBlock& block) {
    // Announce the offer before checking the gate, so the writer can wait for
    // offers in flight before its final drain.
    m_offersInFlight.fetch_add(1);
    if (m_recording.load()) {
        if (block.centerFrequencyHz != m_centerFrequencyHz.load(std::memory_order_relaxed) ||
            block.sampleRateSps != m_sampleRateSps.load(std::memory_order_relaxed)) {
            m_retuned.store(true, std::memory_order_release);
            requestStop();
        } else if (SampleBlock* slot = m_queue.beginWrite()) {
            slot->bytes.assign(block.bytes.begin(), block.bytes.end());  // fits the reserve
            slot->firstSampleUtc = block.firstSampleUtc;
            m_queue.commitWrite();
        } else {
            m_dropped.fetch_add(1, std::memory_order_relaxed);
        }
    }
    m_offersInFlight.fetch_sub(1);
}

RecorderStatus IqRecorder::status() const {
    const std::scoped_lock lock(m_statusMutex);
    RecorderStatus status = m_status;
    status.bytesWritten = m_bytesWritten.load(std::memory_order_relaxed);
    status.droppedBlocks = m_dropped.load(std::memory_order_relaxed);
    status.endedByRetune = m_retuned.load(std::memory_order_acquire);
    return status;
}

void IqRecorder::waitUntilIdle() {
    if (m_writer.joinable()) {
        m_writer.join();
    }
}

void IqRecorder::run(std::filesystem::path folder, IqMetadata metadata) {
    std::string error;
    std::filesystem::path path;
    {
        const std::scoped_lock lock(m_statusMutex);
        path = m_status.path;
    }

    auto fail = [this](std::string message) {
        m_recording.store(false);
        // Wait for the DSP thread to leave offer(), then empty the queue so
        // the next recording starts clean.
        while (m_offersInFlight.load() != 0) {
            std::this_thread::yield();
        }
        while (m_queue.beginRead() != nullptr) {
            m_queue.commitRead();
        }
        const std::scoped_lock lock(m_statusMutex);
        m_status.error = std::move(message);
        m_status.state = RecorderState::Idle;
    };

    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) {
        fail("cannot create " + folder.string() + ": " + ec.message());
        return;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out || !writeSidecar(path, metadata, error)) {
        fail(error.empty() ? "cannot create " + path.string() : error);
        return;
    }

    bool firstBlock = true;
    auto drain = [&]() -> bool {
        while (SampleBlock* block = m_queue.beginRead()) {
            if (firstBlock) {
                metadata.startUtc = block->firstSampleUtc;
                firstBlock = false;
            }
            out.write(reinterpret_cast<const char*>(block->bytes.data()),
                      static_cast<std::streamsize>(block->bytes.size()));
            m_bytesWritten.fetch_add(block->bytes.size(), std::memory_order_relaxed);
            m_queue.commitRead();
            if (!out) {
                return false;
            }
        }
        return true;
    };

    while (!m_stopRequested.load(std::memory_order_acquire)) {
        if (!drain()) {
            fail("writing " + path.filename().string() + " failed (is the disk full?)");
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    {
        const std::scoped_lock lock(m_statusMutex);
        m_status.state = RecorderState::Finishing;
    }
    // Final drain, after the last offer() has left.
    while (m_offersInFlight.load() != 0) {
        std::this_thread::yield();
    }
    const bool ok = drain();
    out.close();
    metadata.sampleCount = m_bytesWritten.load() / 2;
    if (!ok || !writeSidecar(path, metadata, error)) {
        fail(ok ? error : "writing " + path.filename().string() + " failed (is the disk full?)");
        return;
    }
    const std::scoped_lock lock(m_statusMutex);
    m_status.state = RecorderState::Idle;
}

}  // namespace zarescope::device
