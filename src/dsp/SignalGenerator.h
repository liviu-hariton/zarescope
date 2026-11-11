// SPDX-License-Identifier: GPL-3.0-or-later
//
// Synthetic cu8 IQ for tests and demo recordings: a sum of carriers (plain
// or frequency-modulated by a sine) plus Gaussian noise. Lets the whole
// receive chain run, and be checked, without a receiver.

#pragma once

#include <cstdint>
#include <random>
#include <span>

namespace zarescope::dsp {

struct Carrier {
    double offsetHz = 0.0;     // from the centre frequency
    float amplitude = 0.1F;    // 1.0 = full scale
    double deviationHz = 0.0;  // FM peak deviation; 0 = no FM
    double toneHz = 1000.0;    // the modulating tone (FM or AM)
    float amDepth = 0.0F;      // AM modulation depth 0…1; 0 = no AM
};

class SignalGenerator {
public:
    SignalGenerator(uint32_t sampleRateSps, float noiseRms, uint32_t seed = 1);

    // Fills `iq` (an even number of cu8 bytes) with the next samples. The
    // signal is a function of the absolute sample index, so consecutive
    // calls continue seamlessly.
    void generate(std::span<const Carrier> carriers, std::span<uint8_t> iq);

    [[nodiscard]] uint64_t samplesGenerated() const { return m_sampleIndex; }

private:
    uint32_t m_sampleRateSps;
    float m_noiseRms;
    std::mt19937 m_rng;
    std::normal_distribution<float> m_noise{0.0F, 1.0F};
    uint64_t m_sampleIndex = 0;
};

}  // namespace zarescope::dsp
