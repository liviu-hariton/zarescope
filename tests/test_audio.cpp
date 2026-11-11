// SPDX-License-Identifier: GPL-3.0-or-later
//
// The audio chain (M2): filters, shifter, resampler, de-emphasis, and the
// whole demodulator on synthetic stations. Includes M2's acceptance test: a
// synthetic FM signal modulated by a tone demodulates to that tone.

#include "dsp/AudioDemodulator.h"
#include "dsp/FftEngine.h"
#include "dsp/Filters.h"
#include "dsp/SignalGenerator.h"
#include "dsp/Window.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

using namespace zarescope::dsp;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr uint32_t kAudioRate = 48'000;

// |H(f)| of real FIR taps.
double response(const std::vector<float>& taps, double frequencyHz, double rateHz) {
    std::complex<double> sum{};
    for (size_t n = 0; n < taps.size(); ++n) {
        sum += static_cast<double>(taps[n]) *
               std::polar(1.0, -2.0 * kPi * frequencyHz / rateHz * static_cast<double>(n));
    }
    return std::abs(sum);
}

struct Tone {
    double frequencyHz;
    double amplitude;  // peak
};

// The strongest tone in `audio` (a 4096-point windowed FFT of its last
// samples), with a peak amplitude estimate.
Tone dominantTone(const std::vector<float>& audio, double rateHz) {
    constexpr size_t kN = 4096;
    REQUIRE(audio.size() >= kN);
    const auto window = blackmanHarris(kN);
    std::vector<Complex> in(kN);
    std::vector<Complex> out(kN);
    for (size_t i = 0; i < kN; ++i) {
        in[i] = {audio[audio.size() - kN + i] * window[i], 0.0F};
    }
    makeFftEngine(kN)->forward(in, out);
    size_t best = 1;
    for (size_t k = 1; k < kN / 2; ++k) {
        if (std::abs(out[k]) > std::abs(out[best])) {
            best = k;
        }
    }
    // A real sine of peak A gives |X| = A·Σw/2 in its bin.
    const double amplitude = 2.0 * std::abs(out[best]) / windowSum(window);
    return {static_cast<double>(best) * rateHz / kN, amplitude};
}

// Runs `seconds` of the given carriers through a demodulator, block by
// block as the receiver does, and returns the audio.
std::vector<float> demodulate(const std::vector<Carrier>& carriers, const DemodSettings& settings,
                              double seconds, uint32_t inputRate = 2'400'000,
                              float noise = 0.003F) {
    SignalGenerator generator(inputRate, noise, 5);
    AudioDemodulator demod;
    demod.configure(inputRate, kAudioRate, settings);
    std::vector<uint8_t> block(65'536);
    std::vector<float> audio;
    std::vector<float> chunk(demod.maxAudioSamples(block.size()));
    const auto blocks = static_cast<size_t>(seconds * inputRate * 2 / block.size());
    for (size_t b = 0; b < blocks; ++b) {
        generator.generate(carriers, block);
        const size_t n = demod.process(block, chunk);
        audio.insert(audio.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(n));
    }
    return audio;
}

}  // namespace

TEST_CASE("fir: low-pass design has unity DC gain and a real stopband") {
    const double rate = 480'000.0;
    const auto taps = designLowPass(50'000.0, rate, tapsForTransition(40'000.0, rate));
    CHECK(taps.size() % 2 == 1);
    CHECK(response(taps, 0.0, rate) == doctest::Approx(1.0).epsilon(1e-4));
    CHECK(response(taps, 30'000.0, rate) > 0.98);                                 // passband
    CHECK(response(taps, 50'000.0, rate) == doctest::Approx(0.5).epsilon(0.05));  // −6 dB at cutoff
    CHECK(response(taps, 90'000.0, rate) < 1e-3);  // stopband: < −60 dB
    CHECK(tapsForTransition(40'000.0, rate) == 67);
    CHECK(tapsForTransition(1.0, rate) == 255);  // clamped
}

TEST_CASE("fir: decimation keeps one output in N and passes DC unchanged") {
    DecimatingFir<float> fir(designLowPass(1'000.0, 48'000.0, 31), 4);
    std::vector<float> in(400, 0.5F);
    std::vector<float> out(101);
    const size_t n = fir.process(in, out);
    CHECK(n == 100);
    CHECK(out[99] == doctest::Approx(0.5).epsilon(1e-4));
    // Split calls produce the same stream.
    DecimatingFir<float> split(designLowPass(1'000.0, 48'000.0, 31), 4);
    std::vector<float> a(51);
    std::vector<float> b(51);
    const size_t na = split.process(std::span<const float>(in).first(201), a);
    const size_t nb = split.process(std::span<const float>(in).subspan(201), b);
    CHECK(na + nb == 100);
}

TEST_CASE("shifter: a tone at +offset moves to 0 Hz") {
    constexpr double kRate = 1'000'000.0;
    FrequencyShifter shifter;
    shifter.configure(100'000.0, kRate);
    std::vector<Complex> samples(10'000);
    for (size_t n = 0; n < samples.size(); ++n) {
        samples[n] = std::polar(
            1.0F, static_cast<float>(2.0 * kPi * 100'000.0 * static_cast<double>(n) / kRate));
    }
    shifter.process(samples);
    // At 0 Hz the phase no longer turns: every sample equals the first.
    for (size_t n = 1; n < samples.size(); n += 997) {
        CHECK(std::abs(samples[n] - samples[0]) < 1e-3F);
    }
}

TEST_CASE("resampler: the tone keeps its frequency and the count follows the ratio") {
    constexpr double kIn = 50'000.0;
    std::vector<float> in(50'000);
    for (size_t n = 0; n < in.size(); ++n) {
        in[n] =
            static_cast<float>(0.5 * std::sin(2.0 * kPi * 1000.0 * static_cast<double>(n) / kIn));
    }
    Resampler resampler;
    resampler.configure(kIn, kAudioRate);
    std::vector<float> out(60'000);
    const size_t n = resampler.process(in, out);
    CHECK(static_cast<double>(n) == doctest::Approx(48'000.0).epsilon(0.001));
    out.resize(n);
    const Tone tone = dominantTone(out, kAudioRate);
    CHECK(tone.frequencyHz == doctest::Approx(1000.0).epsilon(0.015));
    CHECK(tone.amplitude == doctest::Approx(0.5).epsilon(0.02));
}

TEST_CASE("de-emphasis: 50 µs is −3 dB at 3.18 kHz") {
    Deemphasis deemphasis;
    deemphasis.configure(50.0, 480'000.0);            // a high rate, close to the analogue filter
    const double corner = 1.0 / (2.0 * kPi * 50e-6);  // 3183 Hz
    std::vector<float> samples(48'000);
    for (size_t n = 0; n < samples.size(); ++n) {
        samples[n] =
            static_cast<float>(std::sin(2.0 * kPi * corner * static_cast<double>(n) / 480'000.0));
    }
    deemphasis.process(samples);
    const float peak = *std::max_element(samples.end() - 2'000, samples.end());
    CHECK(peak == doctest::Approx(1.0 / std::sqrt(2.0)).epsilon(0.02));
}

TEST_CASE("demod: wideband FM gives back its modulating tone (M2 acceptance)") {
    // A broadcast station 300 kHz above the centre: ±75 kHz deviation by a
    // 1 kHz tone, so full-scale audio.
    const std::vector<Carrier> station{
        {.offsetHz = 300'000.0, .amplitude = 0.3F, .deviationHz = 75'000.0, .toneHz = 1000.0},
        // A neighbour 400 kHz away that the channel filter must reject.
        {.offsetHz = -100'000.0, .amplitude = 0.3F, .deviationHz = 75'000.0, .toneHz = 2500.0},
    };
    DemodSettings settings{
        .mode = DemodMode::Wfm, .offsetHz = 300'000.0, .deemphasisUs = 0.0, .volume = 1.0F};
    const auto audio = demodulate(station, settings, 0.6);
    CHECK(static_cast<double>(audio.size()) == doctest::Approx(0.6 * kAudioRate).epsilon(0.05));
    const Tone tone = dominantTone(audio, kAudioRate);
    CHECK(tone.frequencyHz == doctest::Approx(1000.0).epsilon(0.015));
    CHECK(tone.amplitude == doctest::Approx(1.0).epsilon(0.1));

    // Tuned to the neighbour instead, we hear its 2.5 kHz tone.
    settings.offsetHz = -100'000.0;
    CHECK(dominantTone(demodulate(station, settings, 0.6), kAudioRate).frequencyHz ==
          doctest::Approx(2500.0).epsilon(0.01));
}

TEST_CASE("demod: de-emphasis softens high audio frequencies") {
    const std::vector<Carrier> station{
        {.offsetHz = 0.0, .amplitude = 0.3F, .deviationHz = 75'000.0, .toneHz = 10'000.0}};
    DemodSettings flat{.mode = DemodMode::Wfm, .deemphasisUs = 0.0, .volume = 1.0F};
    DemodSettings europe{.mode = DemodMode::Wfm, .deemphasisUs = 50.0, .volume = 1.0F};
    const double without = dominantTone(demodulate(station, flat, 0.4), kAudioRate).amplitude;
    const double with = dominantTone(demodulate(station, europe, 0.4), kAudioRate).amplitude;
    // At 10 kHz a 50 µs network attenuates by about 10.4 dB (×0.30).
    CHECK(with / without == doctest::Approx(0.30).epsilon(0.1));
}

TEST_CASE("demod: narrowband FM and AM give back their tones") {
    SUBCASE("NFM, ±3 kHz by 800 Hz") {
        const std::vector<Carrier> station{
            {.offsetHz = -200'000.0, .amplitude = 0.2F, .deviationHz = 3'000.0, .toneHz = 800.0}};
        DemodSettings settings{.mode = DemodMode::Nfm, .offsetHz = -200'000.0, .volume = 1.0F};
        const Tone tone = dominantTone(demodulate(station, settings, 0.5), kAudioRate);
        CHECK(tone.frequencyHz == doctest::Approx(800.0).epsilon(0.02));
        CHECK(tone.amplitude == doctest::Approx(0.6).epsilon(0.15));  // 3/5 of full scale
    }
    SUBCASE("AM, 50 % by 600 Hz, on a 2.048 MS/s capture") {
        const std::vector<Carrier> station{
            {.offsetHz = 125'000.0, .amplitude = 0.2F, .toneHz = 600.0, .amDepth = 0.5F}};
        DemodSettings settings{.mode = DemodMode::Am, .offsetHz = 125'000.0, .volume = 1.0F};
        const Tone tone = dominantTone(demodulate(station, settings, 0.8, 2'048'000), kAudioRate);
        CHECK(tone.frequencyHz == doctest::Approx(600.0).epsilon(0.02));
        CHECK(tone.amplitude > 0.2);
    }
}

TEST_CASE("demod: the squelch silences noise and opens on a signal") {
    DemodSettings settings{.mode = DemodMode::Nfm, .squelchDb = -40.0F, .volume = 1.0F};
    // Noise only: the channel stays below −40 dBFS.
    const auto quiet = demodulate({}, settings, 0.3, 2'400'000, 0.003F);
    float loudest = 0.0F;
    for (float s : quiet) {
        loudest = std::max(loudest, std::fabs(s));
    }
    CHECK(loudest < 1e-3F);
    // A strong carrier opens it.
    const std::vector<Carrier> station{
        {.offsetHz = 0.0, .amplitude = 0.3F, .deviationHz = 3'000.0, .toneHz = 1000.0}};
    const auto open = demodulate(station, settings, 0.3);
    CHECK(dominantTone(open, kAudioRate).amplitude > 0.3);
}

TEST_CASE("demod: Off produces nothing; defaults and ranges are sane") {
    AudioDemodulator demod;
    demod.configure(2'400'000, kAudioRate, {});
    std::vector<uint8_t> block(1024, 127);
    std::vector<float> audio(16);
    CHECK(demod.process(block, audio) == 0);
    CHECK(demod.maxAudioSamples(65'536) == 0);
    for (DemodMode mode : {DemodMode::Wfm, DemodMode::Nfm, DemodMode::Am}) {
        CHECK(minBandwidthHz(mode) <= defaultBandwidthHz(mode));
        CHECK(defaultBandwidthHz(mode) <= maxBandwidthHz(mode));
    }
}
