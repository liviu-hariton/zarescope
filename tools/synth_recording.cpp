// SPDX-License-Identifier: GPL-3.0-or-later
//
// zarescope-synth: writes a synthetic .cu8 recording with its sidecar, so the
// app (and anyone working on it) can see a realistic spectrum and waterfall
// without a receiver.
//
//   build/tools/zarescope-synth OUT.cu8 [seconds]
//   open build/Zarescope.app --args --open OUT.cu8
//
// The signal imitates the FM broadcast band around 100 MHz at 2.4 MS/s:
// a few wideband FM stations of different strengths (75 kHz deviation),
// one that fades in and out, a narrow unmodulated carrier and receiver noise.

#include "device/IqMetadata.h"
#include "dsp/SignalGenerator.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace zarescope;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s OUT.cu8 [seconds]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path path = argv[1];
    const double seconds = argc > 2 ? std::stod(argv[2]) : 20.0;
    constexpr uint32_t kRate = 2'400'000;
    constexpr int64_t kCenter = 100'000'000;

    // Stations at their offsets from 100.000 MHz, like a real FM dial.
    const std::vector<dsp::Carrier> stations{
        {.offsetHz = -900'000.0, .amplitude = 0.10F, .deviationHz = 75'000.0, .toneHz = 900.0},
        {.offsetHz = -400'000.0, .amplitude = 0.30F, .deviationHz = 75'000.0, .toneHz = 1300.0},
        {.offsetHz = 300'000.0, .amplitude = 0.05F, .deviationHz = 60'000.0, .toneHz = 700.0},
        {.offsetHz = 800'000.0, .amplitude = 0.18F, .deviationHz = 75'000.0, .toneHz = 2100.0},
        {.offsetHz = 120'000.0, .amplitude = 0.02F},  // a narrow carrier
    };
    // One more station that fades in and out every few seconds.
    dsp::Carrier fading{
        .offsetHz = -150'000.0, .amplitude = 0.0F, .deviationHz = 50'000.0, .toneHz = 400.0};

    dsp::SignalGenerator generator(kRate, 0.004F, 42);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", path.string().c_str());
        return 1;
    }
    // Generate in 0.1-s chunks so the fading station can change strength.
    const auto chunkSamples = static_cast<size_t>(kRate / 10);
    std::vector<uint8_t> chunk(chunkSamples * 2);
    const auto chunks = static_cast<size_t>(seconds * 10.0);
    for (size_t i = 0; i < chunks; ++i) {
        const double t = static_cast<double>(i) / 10.0;
        const double phase = std::fmod(t, 6.0) / 6.0;  // 6-s cycle
        fading.amplitude = static_cast<float>(0.12 * std::max(0.0, std::sin(phase * 3.14159265)));
        std::vector<dsp::Carrier> carriers = stations;
        carriers.push_back(fading);
        generator.generate(carriers, chunk);
        out.write(reinterpret_cast<const char*>(chunk.data()),
                  static_cast<std::streamsize>(chunk.size()));
    }
    out.close();

    device::IqMetadata metadata;
    metadata.centerFrequencyHz = kCenter;
    metadata.sampleRateSps = kRate;
    metadata.startUtc = std::chrono::system_clock::now();
    metadata.device = "synthetic (zarescope-synth)";
    metadata.software = "zarescope-synth";
    metadata.sampleCount = generator.samplesGenerated();
    std::string error;
    if (!device::writeSidecar(path, metadata, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("wrote %s (%.1f s, %.0f MB) and its sidecar\n", path.string().c_str(), seconds,
                static_cast<double>(generator.samplesGenerated() * 2) / 1e6);
    return 0;
}
