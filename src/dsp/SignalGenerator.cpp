// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/SignalGenerator.h"

#include "dsp/IqConvert.h"

#include <cmath>
#include <numbers>

namespace zarescope::dsp {

SignalGenerator::SignalGenerator(uint32_t sampleRateSps, float noiseRms, uint32_t seed)
    : m_sampleRateSps(sampleRateSps), m_noiseRms(noiseRms), m_rng(seed) {}

void SignalGenerator::generate(std::span<const Carrier> carriers, std::span<uint8_t> iq) {
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    const double rate = static_cast<double>(m_sampleRateSps);
    for (size_t i = 0; i + 1 < iq.size(); i += 2) {
        const double t = static_cast<double>(m_sampleIndex) / rate;
        double re = 0.0;
        double im = 0.0;
        for (const Carrier& c : carriers) {
            // FM by a sine: φ(t) = 2π·f·t + (Δf / fm)·sin(2π·fm·t).
            double phase = kTwoPi * c.offsetHz * t;
            if (c.deviationHz > 0.0 && c.toneHz > 0.0) {
                phase += (c.deviationHz / c.toneHz) * std::sin(kTwoPi * c.toneHz * t);
            }
            double amplitude = c.amplitude;
            if (c.amDepth > 0.0F) {
                amplitude *= 1.0 + c.amDepth * std::sin(kTwoPi * c.toneHz * t);
            }
            re += amplitude * std::cos(phase);
            im += amplitude * std::sin(phase);
        }
        re += m_noiseRms * m_noise(m_rng);
        im += m_noiseRms * m_noise(m_rng);
        iq[i] = floatToCu8(static_cast<float>(re));
        iq[i + 1] = floatToCu8(static_cast<float>(im));
        ++m_sampleIndex;
    }
}

}  // namespace zarescope::dsp
