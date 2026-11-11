// SPDX-License-Identifier: GPL-3.0-or-later
//
// zarescope-synth: writes a synthetic .cu8 recording with its sidecar, so the
// app (and anyone working on it) can see a realistic spectrum and waterfall
// without a receiver.
//
//   build/tools/zarescope-synth OUT.cu8 [seconds]
//   build/tools/zarescope-synth OUT.cu8 [seconds] --adsb [--station LAT,LON]
//   open build/Zarescope.app --args --open OUT.cu8
//
// Default: the FM broadcast band around 100 MHz at 2.4 MS/s: a few wideband
// FM stations of different strengths (75 kHz deviation), one that fades in
// and out, a narrow unmodulated carrier and receiver noise.
//
// --adsb: 1090 MHz at 2 MS/s with six aircraft flying around the station
// (default 44.4268 N, 26.1025 E), each sending positions twice a second,
// velocity every second and its callsign every five, weaker with distance.
// Nothing here is transmitted: it only writes a file.

#include "decoders/adsb/Cpr.h"
#include "decoders/adsb/Demodulator.h"
#include "decoders/adsb/Encoder.h"
#include "device/IqMetadata.h"
#include "dsp/IqConvert.h"
#include "dsp/SignalGenerator.h"

#include <cmath>
#include <numbers>
#include <random>
#include <string_view>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace zarescope;

namespace {

struct Plane {
    uint32_t icao;
    const char* callsign;
    double bearingDeg;  // from the station, at the start
    double rangeKm;
    double trackDeg;
    double speedKt;
    int altitudeFt;
    int verticalRateFpm;
};

// Moves `from` by `km` along `bearing` (flat-earth step, fine for a demo).
adsb::LatLon move(const adsb::LatLon& from, double bearingDeg, double km) {
    const double b = bearingDeg * std::numbers::pi / 180.0;
    const double dLat = km * std::cos(b) / 111.32;
    const double dLon =
        km * std::sin(b) / (111.32 * std::cos(from.latDeg * std::numbers::pi / 180.0));
    return {from.latDeg + dLat, from.lonDeg + dLon};
}

int writeAdsb(const std::filesystem::path& path, double seconds, const adsb::LatLon& station) {
    constexpr uint32_t kRate = adsb::kSampleRateSps;
    const std::vector<Plane> planes{
        {0x4B1801, "ROT301", 40.0, 60.0, 250.0, 430.0, 35000, 0},
        {0x4B1802, "WZZ4512", 130.0, 120.0, 310.0, 460.0, 37000, -512},
        {0x4B1803, "BLA7", 220.0, 30.0, 45.0, 180.0, 6000, 1280},
        {0x4B1804, "THY1JK", 300.0, 170.0, 120.0, 470.0, 39000, 0},
        {0x4B1805, "LOT34", 10.0, 210.0, 190.0, 440.0, 33000, -1024},
        {0x4B1806, "AUA791", 170.0, 90.0, 350.0, 400.0, 28000, 2048},
    };
    const auto total = static_cast<size_t>(seconds * kRate);
    std::vector<float> re(total);
    std::vector<float> im(total);
    std::mt19937 rng(7);
    std::normal_distribution<float> noise(0.0F, 0.01F);
    for (size_t i = 0; i < total; ++i) {
        re[i] = noise(rng);
        im[i] = noise(rng);
    }
    std::uniform_real_distribution<double> jitter(0.0, 0.05);
    std::uniform_real_distribution<float> phase(0.0F, 6.28F);
    std::vector<uint8_t> burst;
    size_t count = 0;
    for (const Plane& plane : planes) {
        const adsb::LatLon start = move(station, plane.bearingDeg, plane.rangeKm);
        bool odd = false;
        for (double t = jitter(rng); t < seconds; t += 0.5) {
            const double km = plane.speedKt * 1.852 * t / 3600.0;
            const adsb::LatLon here = move(start, plane.trackDeg, km);
            const int altitude =
                plane.altitudeFt + static_cast<int>(plane.verticalRateFpm * t / 60.0);
            std::vector<adsb::RawMessage> messages{
                adsb::encodeAirbornePosition(plane.icao, here.latDeg, here.lonDeg, altitude, odd)};
            odd = !odd;
            const auto step = static_cast<int>(t * 2.0);
            if (step % 2 == 0) {
                messages.push_back(adsb::encodeVelocity(plane.icao, plane.speedKt, plane.trackDeg,
                                                        plane.verticalRateFpm));
            }
            if (step % 10 == 0) {
                messages.push_back(adsb::encodeIdentification(plane.icao, plane.callsign));
            }
            // Weaker with distance: 0.5 close by, 0.05 at 250 km.
            const double distance = adsb::distanceKm(station, here);
            const auto amplitude =
                static_cast<float>(std::clamp(0.5 * std::exp(-distance / 110.0), 0.05, 0.5));
            double at = t;
            for (const auto& message : messages) {
                burst.clear();
                adsb::modulate(message, amplitude, phase(rng), burst);
                const auto first = static_cast<size_t>(at * kRate);
                for (size_t k = 0; k + 1 < burst.size() && first + k / 2 < total; k += 2) {
                    re[first + k / 2] += dsp::cu8ToFloat(burst[k]);
                    im[first + k / 2] += dsp::cu8ToFloat(burst[k + 1]);
                }
                at += 0.002;
                ++count;
            }
        }
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", path.string().c_str());
        return 1;
    }
    std::vector<uint8_t> bytes(total * 2);
    for (size_t i = 0; i < total; ++i) {
        bytes[2 * i] = dsp::floatToCu8(re[i]);
        bytes[2 * i + 1] = dsp::floatToCu8(im[i]);
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();

    device::IqMetadata metadata;
    metadata.centerFrequencyHz = 1'090'000'000;
    metadata.sampleRateSps = kRate;
    metadata.startUtc = std::chrono::system_clock::now();
    metadata.device = "synthetic ADS-B (zarescope-synth)";
    metadata.software = "zarescope-synth";
    metadata.sampleCount = total;
    std::string error;
    if (!device::writeSidecar(path, metadata, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("wrote %s (%.1f s at 2 MS/s, %zu messages from %zu aircraft) and its sidecar\n",
                path.string().c_str(), seconds, count, planes.size());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s OUT.cu8 [seconds] [--adsb [--station LAT,LON]]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path path = argv[1];
    double seconds = 20.0;
    bool adsbMode = false;
    adsb::LatLon station{44.4268, 26.1025};
    for (int i = 2; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--adsb") {
            adsbMode = true;
        } else if (arg == "--station" && i + 1 < argc) {
            if (std::sscanf(argv[++i], "%lf,%lf", &station.latDeg, &station.lonDeg) != 2) {
                std::fprintf(stderr, "--station takes LAT,LON\n");
                return 2;
            }
        } else {
            seconds = std::stod(std::string(arg));
        }
    }
    if (adsbMode) {
        return writeAdsb(path, seconds, station);
    }
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
