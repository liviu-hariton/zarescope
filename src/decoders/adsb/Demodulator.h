// SPDX-License-Identifier: GPL-3.0-or-later
//
// ModeSDemodulator: finds Mode S messages in raw cu8 IQ sampled at
// 2 MS/s (two samples per 1 µs bit, one per 0.5 µs pulse).
//
//   magnitude → preamble (pulses at 0, 1, 3.5 and 4.5 µs, quiet between
//   and after) → 56 or 112 bits, each bit = which half of its microsecond
//   holds the pulse (pulse-position modulation) → CRC check, single-bit
//   correction for DF17/DF18
//
// Accepted: DF17/DF18 (extended squitter, ADS-B) and DF11 with interrogator
// 0 (all-call replies), whose CRC is plain. Formats whose parity is overlaid
// with the address (DF0/4/5/16/20/21) need a list of known aircraft to
// verify, and are dropped for now.
//
// A message can straddle two blocks, so the last samples of each block are
// kept and examined again with the next one.

#pragma once

#include "decoders/adsb/ModeS.h"

#include <cstdint>
#include <span>
#include <vector>

namespace zarescope::adsb {

// The sample rate the demodulator needs.
inline constexpr uint32_t kSampleRateSps = 2'000'000;

class ModeSDemodulator {
public:
    ModeSDemodulator();

    // Feeds a block of cu8 bytes and appends every valid message to `out`
    // (which the caller clears). Allocation-free once the first block of the
    // largest size has been seen.
    void process(std::span<const uint8_t> iq, std::vector<RawMessage>& out);

    // Messages rejected by the CRC since construction (for health display).
    [[nodiscard]] uint64_t rejected() const { return m_rejected; }

private:
    std::vector<float> m_magnitude;  // carried-over tail + this block
    size_t m_carry = 0;
    uint64_t m_rejected = 0;
};

}  // namespace zarescope::adsb
