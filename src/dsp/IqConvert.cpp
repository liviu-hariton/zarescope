// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/IqConvert.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace zarescope::dsp {

namespace {

// A 256-entry table is faster than the arithmetic for every byte.
constexpr std::array<float, 256> kCu8Table = [] {
    std::array<float, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[static_cast<size_t>(i)] = cu8ToFloat(static_cast<uint8_t>(i));
    }
    return table;
}();

}  // namespace

void cu8ToComplex(std::span<const uint8_t> iq, std::span<std::complex<float>> out) {
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = {kCu8Table[iq[2 * i]], kCu8Table[iq[2 * i + 1]]};
    }
}

uint8_t floatToCu8(float value) {
    const float scaled = std::round(value * 127.5F + 127.5F);
    return static_cast<uint8_t>(std::clamp(scaled, 0.0F, 255.0F));
}

}  // namespace zarescope::dsp
