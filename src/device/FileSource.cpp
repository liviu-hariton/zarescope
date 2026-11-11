// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/FileSource.h"

#include <fstream>
#include <stdexcept>

namespace zarescope::device {

FileSource::FileSource(std::filesystem::path recording, FileSourceOptions options)
    : m_path(std::move(recording)), m_options(options) {
    std::string error;
    auto metadata = readSidecar(m_path, error);
    if (!metadata) {
        throw std::runtime_error(error);
    }
    m_metadata = *metadata;
}

FileSource::~FileSource() { stop(); }

bool FileSource::start(BlockQueue& queue, std::string& error) {
    stop();
    std::ifstream probe(m_path, std::ios::binary);
    if (!probe) {
        error = "cannot open " + m_path.string();
        return false;
    }
    resetState();
    m_stopRequested.store(false);
    m_thread = std::thread([this, &queue] { run(queue); });
    return true;
}

void FileSource::stop() {
    m_stopRequested.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

SourceInfo FileSource::info() const {
    return {SourceKind::File, m_path.filename().string(), m_metadata.centerFrequencyHz,
            m_metadata.sampleRateSps};
}

void FileSource::run(BlockQueue& queue) {
    using namespace std::chrono;
    std::ifstream in(m_path, std::ios::binary);
    if (!in) {
        finish("cannot open " + m_path.string());
        return;
    }
    // One block's worth of bytes, allocated once before the loop.
    std::vector<uint8_t> buffer(kBlockBytes);
    const double rate = static_cast<double>(m_metadata.sampleRateSps);
    uint64_t samplesInPass = 0;  // since the start of this pass through the file
    const auto paceStart = steady_clock::now();
    uint64_t samplesPaced = 0;  // since start(), for real-time pacing

    while (!m_stopRequested.load(std::memory_order_relaxed)) {
        in.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
        auto got = static_cast<size_t>(in.gcount());
        got -= got % 2;  // whole I/Q pairs only
        if (got == 0) {
            if (!m_options.loop) {
                finish();
                return;
            }
            in.clear();
            in.seekg(0);
            samplesInPass = 0;
            continue;
        }

        const auto offset = duration_cast<system_clock::duration>(
            duration<double>(static_cast<double>(samplesInPass) / rate));
        const std::span<const uint8_t> bytes(buffer.data(), got);

        if (m_options.realTime) {
            pushBlock(queue, bytes, m_metadata.centerFrequencyHz, m_metadata.sampleRateSps,
                      m_metadata.startUtc + offset);
        } else {
            // As fast as the consumer allows, never dropping: wait for room.
            while (queue.beginWrite() == nullptr) {
                if (m_stopRequested.load(std::memory_order_relaxed)) {
                    return;
                }
                std::this_thread::yield();
            }
            pushBlock(queue, bytes, m_metadata.centerFrequencyHz, m_metadata.sampleRateSps,
                      m_metadata.startUtc + offset);
        }

        const uint64_t samples = got / 2;
        samplesInPass += samples;
        samplesPaced += samples;
        if (m_options.realTime) {
            const auto due = paceStart + duration_cast<steady_clock::duration>(duration<double>(
                                             static_cast<double>(samplesPaced) / rate));
            std::this_thread::sleep_until(due);
        }
    }
}

}  // namespace zarescope::device
