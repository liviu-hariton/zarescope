// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/Window.h"

#include <cmath>
#include <numbers>
#include <numeric>

namespace zarescope::dsp {

std::vector<float> blackmanHarris(size_t length) {
    constexpr double kA0 = 0.35875;
    constexpr double kA1 = 0.48829;
    constexpr double kA2 = 0.14128;
    constexpr double kA3 = 0.01168;
    std::vector<float> window(length);
    const double n = static_cast<double>(length);
    for (size_t i = 0; i < length; ++i) {
        const double x = 2.0 * std::numbers::pi * static_cast<double>(i) / n;
        window[i] = static_cast<float>(kA0 - kA1 * std::cos(x) + kA2 * std::cos(2.0 * x) -
                                       kA3 * std::cos(3.0 * x));
    }
    return window;
}

float windowSum(std::span<const float> window) {
    return std::accumulate(window.begin(), window.end(), 0.0F);
}

}  // namespace zarescope::dsp
