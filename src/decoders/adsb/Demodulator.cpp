// SPDX-License-Identifier: GPL-3.0-or-later

#include "decoders/adsb/Demodulator.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace zarescope::adsb {

namespace {

// Preamble (16 samples = 8 µs) + the longest message (112 bits × 2).
constexpr size_t kPreambleSamples = 16;
constexpr size_t kLongSamples = kPreambleSamples + kLongBytes * 8 * 2;

// (byte − 127.5)², for the magnitude.
constexpr std::array<float, 256> kSquare = [] {
    std::array<float, 256> table{};
    for (int i = 0; i < 256; ++i) {
        const float v = (static_cast<float>(i) - 127.5F) / 127.5F;
        table[static_cast<size_t>(i)] = v * v;
    }
    return table;
}();

}  // namespace

ModeSDemodulator::ModeSDemodulator() { m_magnitude.reserve(65'536 / 2 + kLongSamples); }

void ModeSDemodulator::process(std::span<const uint8_t> iq, std::vector<RawMessage>& out) {
    const size_t samples = iq.size() / 2;
    m_magnitude.resize(m_carry + samples);
    for (size_t i = 0; i < samples; ++i) {
        m_magnitude[m_carry + i] = std::sqrt(kSquare[iq[2 * i]] + kSquare[iq[2 * i + 1]]);
    }
    const std::vector<float>& m = m_magnitude;
    const size_t total = m.size();

    size_t j = 0;
    for (; j + kLongSamples <= total; ++j) {
        // The four preamble pulses, each stronger than its neighbours.
        if (!(m[j] > m[j + 1] && m[j + 1] < m[j + 2] && m[j + 2] > m[j + 3] && m[j + 3] < m[j] &&
              m[j + 4] < m[j] && m[j + 5] < m[j] && m[j + 6] < m[j] && m[j + 7] > m[j + 8] &&
              m[j + 8] < m[j + 9] && m[j + 9] > m[j + 6])) {
            continue;
        }
        // Quiet between the pulse pairs and before the data.
        const float high = (m[j] + m[j + 2] + m[j + 7] + m[j + 9]) / 6.0F;
        if (m[j + 4] >= high || m[j + 5] >= high || m[j + 11] >= high || m[j + 12] >= high ||
            m[j + 13] >= high || m[j + 14] >= high) {
            continue;
        }

        // Pulse-position bits: a 1 has its pulse in the first half.
        RawMessage message;
        const size_t data = j + kPreambleSamples;
        for (size_t bit = 0; bit < kLongBytes * 8; ++bit) {
            const float first = m[data + 2 * bit];
            const float second = m[data + 2 * bit + 1];
            if (first > second) {
                message.bytes[bit / 8] |= static_cast<uint8_t>(0x80U >> (bit % 8));
            }
        }
        const int df = downlinkFormat(message.bytes);
        message.lengthBytes = messageBytes(df);
        const std::span<uint8_t> bytes(message.bytes.data(), message.lengthBytes);

        bool valid = false;
        if (df == 17 || df == 18) {
            valid = crcRemainder(bytes) == 0;
            if (!valid && correctSingleBit(bytes)) {
                valid = true;
                message.corrected = true;
            }
        } else if (df == 11) {
            valid = crcRemainder(bytes) == 0;
        }
        if (!valid) {
            if (df == 17 || df == 18 || df == 11) {
                ++m_rejected;
            }
            continue;
        }
        const float level = (m[j] + m[j + 2] + m[j + 7] + m[j + 9]) / 4.0F;
        message.signalDb = 20.0F * std::log10(std::max(level, 1e-6F));
        out.push_back(message);
        // Skip past this message.
        j += kPreambleSamples + message.lengthBytes * 16 - 1;
    }

    // Keep the samples a message starting from here could still need.
    const size_t keep = std::min(total - std::min(j, total), kLongSamples);
    std::copy(m_magnitude.end() - static_cast<std::ptrdiff_t>(keep), m_magnitude.end(),
              m_magnitude.begin());
    m_carry = keep;
}

}  // namespace zarescope::adsb
