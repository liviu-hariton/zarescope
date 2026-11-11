// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/SampleSource.h"

#include <algorithm>

namespace zarescope::device {

bool SampleSource::pushBlock(BlockQueue& queue, std::span<const uint8_t> bytes,
                             int64_t centerFrequencyHz, uint32_t sampleRateSps,
                             std::chrono::system_clock::time_point firstSampleUtc) {
    const uint64_t sequence = m_sequence++;
    SampleBlock* block = queue.beginWrite();
    if (block == nullptr) {
        m_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const size_t size = std::min(bytes.size(), kBlockBytes);
    block->bytes.resize(size);  // within the reserved capacity: no allocation
    std::copy_n(bytes.begin(), size, block->bytes.begin());
    block->centerFrequencyHz = centerFrequencyHz;
    block->sampleRateSps = sampleRateSps;
    block->firstSampleUtc = firstSampleUtc;
    block->sequence = sequence;
    queue.commitWrite();
    return true;
}

void SampleSource::finish(std::string error) {
    if (!m_errorReady.load(std::memory_order_acquire)) {
        m_error = std::move(error);
        m_errorReady.store(true, std::memory_order_release);
    }
    m_finished.store(true, std::memory_order_release);
}

void SampleSource::resetState() {
    m_finished.store(false, std::memory_order_release);
    m_errorReady.store(false, std::memory_order_release);
    m_error.clear();
    m_dropped.store(0, std::memory_order_relaxed);
    m_sequence = 0;
}

std::string SampleSource::lastError() const {
    return m_errorReady.load(std::memory_order_acquire) ? m_error : std::string{};
}

}  // namespace zarescope::device
