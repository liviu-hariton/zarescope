// SPDX-License-Identifier: GPL-3.0-or-later
//
// Conversions of raw RTL-SDR samples.
//
// The RTL2832U delivers "cu8": interleaved I and Q bytes, unsigned, with
// zero at 127.5. One complex sample is two bytes.

#pragma once

#include <complex>
#include <cstdint>
#include <span>

namespace zarescope::dsp {

// cu8 byte → [−1, +1].
constexpr float cu8ToFloat(uint8_t value) { return (static_cast<float>(value) - 127.5F) / 127.5F; }

// Converts `out.size()` complex samples from `iq` (which holds at least
// twice as many bytes). Never allocates.
void cu8ToComplex(std::span<const uint8_t> iq, std::span<std::complex<float>> out);

// [−1, +1] → cu8 byte, rounded and clamped. For synthetic test signals.
uint8_t floatToCu8(float value);

}  // namespace zarescope::dsp
