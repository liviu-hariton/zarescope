// SPDX-License-Identifier: GPL-3.0-or-later
//
// The DSP core: the SPSC queue, the window, the FFT, sample conversion and
// the spectrum analyser. Includes M1's acceptance test: a synthetic tone at a
// known offset peaks in the right FFT bin.

#include "dsp/FftEngine.h"
#include "dsp/IqConvert.h"
#include "dsp/SignalGenerator.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/SpscSlots.h"
#include "dsp/Window.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <thread>
#include <vector>

using namespace zarescope::dsp;

namespace {

// Collects every row the analyser produces.
struct RowCollector final : SpectrumRowSink {
    std::vector<std::vector<float>> rows;
    void onSpectrumRow(std::span<const float> dbfs) override {
        rows.emplace_back(dbfs.begin(), dbfs.end());
    }
};

size_t peakIndex(const std::vector<float>& row) {
    return static_cast<size_t>(
        std::distance(row.begin(), std::max_element(row.begin(), row.end())));
}

}  // namespace

TEST_CASE("spsc: fills, refuses when full, drains in order") {
    SpscSlots<int> queue(3);  // rounded up to 4
    CHECK(queue.capacity() == 4);
    for (int i = 0; i < 4; ++i) {
        int* slot = queue.beginWrite();
        REQUIRE(slot != nullptr);
        *slot = i;
        queue.commitWrite();
    }
    CHECK(queue.beginWrite() == nullptr);
    CHECK(queue.sizeApprox() == 4);
    for (int i = 0; i < 4; ++i) {
        int* slot = queue.beginRead();
        REQUIRE(slot != nullptr);
        CHECK(*slot == i);
        queue.commitRead();
    }
    CHECK(queue.beginRead() == nullptr);
}

TEST_CASE("spsc: one producer and one consumer thread see every item once, in order") {
    SpscSlots<uint64_t> queue(64);
    constexpr uint64_t kCount = 200'000;
    std::thread producer([&] {
        for (uint64_t i = 0; i < kCount;) {
            if (uint64_t* slot = queue.beginWrite()) {
                *slot = i++;
                queue.commitWrite();
            }
        }
    });
    uint64_t expected = 0;
    bool inOrder = true;
    while (expected < kCount) {
        if (uint64_t* slot = queue.beginRead()) {
            inOrder = inOrder && *slot == expected;
            ++expected;
            queue.commitRead();
        }
    }
    producer.join();
    CHECK(inOrder);
}

TEST_CASE("window: Blackman-Harris is symmetric, zero at the edge, one at the centre") {
    const auto w = blackmanHarris(1024);
    REQUIRE(w.size() == 1024);
    CHECK(w[0] == doctest::Approx(0.00006).epsilon(0.01));  // a0 − a1 + a2 − a3
    CHECK(w[512] == doctest::Approx(1.0).epsilon(1e-6));
    for (size_t i = 1; i < 512; ++i) {
        CHECK(w[i] == doctest::Approx(w[1024 - i]).epsilon(1e-5));
    }
    // Coherent gain a0 = 0.35875.
    CHECK(windowSum(w) / 1024.0F == doctest::Approx(0.35875).epsilon(1e-4));
}

TEST_CASE("fft: a complex exponential lands in one bin with magnitude N") {
    constexpr size_t kN = 256;
    auto fft = makeFftEngine(kN);
    std::vector<std::complex<float>> in(kN);
    std::vector<std::complex<float>> out(kN);
    const int k = 37;
    for (size_t n = 0; n < kN; ++n) {
        const double phase = 2.0 * std::numbers::pi * k * static_cast<double>(n) / kN;
        in[n] = {static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase))};
    }
    fft->forward(in, out);
    CHECK(std::abs(out[k]) == doctest::Approx(kN).epsilon(1e-4));
    for (size_t i = 0; i < kN; ++i) {
        if (static_cast<int>(i) != k) {
            CHECK(std::abs(out[i]) < 1e-2F);
        }
    }
}

TEST_CASE("iq: cu8 conversion is centred on 127.5 and round-trips") {
    CHECK(cu8ToFloat(0) == doctest::Approx(-1.0));
    CHECK(cu8ToFloat(255) == doctest::Approx(1.0));
    CHECK(cu8ToFloat(127) < 0.0F);
    CHECK(cu8ToFloat(128) > 0.0F);
    for (int v = 0; v < 256; ++v) {
        CHECK(floatToCu8(cu8ToFloat(static_cast<uint8_t>(v))) == v);
    }
    const std::array<uint8_t, 4> raw{0, 255, 128, 127};
    std::array<std::complex<float>, 2> out{};
    cu8ToComplex(raw, out);
    CHECK(out[0].real() == doctest::Approx(-1.0));
    CHECK(out[0].imag() == doctest::Approx(1.0));
}

TEST_CASE("spectrum: a synthetic tone peaks in the right bin at the right level (M1 acceptance)") {
    constexpr uint32_t kRate = 2'400'000;
    constexpr size_t kFft = 4096;
    const double binHz = static_cast<double>(kRate) / kFft;  // 585.9375 Hz

    for (const double offsetHz : {300'000.0, -450'000.0, 703'125.0, 0.0}) {
        CAPTURE(offsetHz);
        SpectrumAnalyzer analyzer({.fftSize = kFft, .rowsPerSecond = 30.0});
        analyzer.setSampleRate(kRate);
        CHECK(analyzer.binWidthHz() == doctest::Approx(binHz));

        SignalGenerator generator(kRate, 0.01F);
        const std::array carriers{Carrier{.offsetHz = offsetHz, .amplitude = 0.5F}};
        std::vector<uint8_t> iq(kRate / 5 * 2);  // 200 ms
        generator.generate(carriers, iq);

        RowCollector rows;
        analyzer.process(iq, rows);
        REQUIRE(rows.rows.size() >= 5);  // 30 rows/s × 0.2 s, rounded

        // Screen index: N/2 is the centre; each bin is binHz to the right.
        const auto expected =
            static_cast<size_t>(static_cast<long>(kFft / 2) + std::lround(offsetHz / binHz));
        const auto& row = rows.rows.back();
        CHECK(peakIndex(row) == expected);
        // Amplitude 0.5 → −6.02 dBFS. The tones above sit on bin centres.
        CHECK(row[expected] == doctest::Approx(-6.02).epsilon(0.05));
    }
}

TEST_CASE("spectrum: frames per row follow the sample rate and the row rate") {
    SpectrumAnalyzer analyzer({.fftSize = 4096, .rowsPerSecond = 30.0});
    analyzer.setSampleRate(2'400'000);
    CHECK(analyzer.framesPerRow() == 20);  // 585.9 frames/s ÷ 30
    analyzer.setSampleRate(1'024'000);
    CHECK(analyzer.framesPerRow() == 8);
    SpectrumAnalyzer fixed({.fftSize = 1024, .rowsPerSecond = 30.0, .framesPerRow = 3});
    fixed.setSampleRate(2'400'000);
    CHECK(fixed.framesPerRow() == 3);
}

TEST_CASE("spectrum: input split across calls gives the same rows as one call") {
    constexpr uint32_t kRate = 1'024'000;
    SignalGenerator generator(kRate, 0.05F, 7);
    const std::array carriers{
        Carrier{.offsetHz = 100'000.0, .amplitude = 0.3F, .deviationHz = 5000.0}};
    std::vector<uint8_t> iq(kRate / 10 * 2);
    generator.generate(carriers, iq);

    const SpectrumConfig config{.fftSize = 2048, .rowsPerSecond = 30.0};
    SpectrumAnalyzer whole(config);
    whole.setSampleRate(kRate);
    RowCollector a;
    whole.process(iq, a);

    SpectrumAnalyzer pieces(config);
    pieces.setSampleRate(kRate);
    RowCollector b;
    // Odd-sized pieces that don't line up with FFT frames.
    for (size_t pos = 0; pos < iq.size();) {
        const size_t take = std::min<size_t>(2 * 777, iq.size() - pos);
        pieces.process(std::span(iq).subspan(pos, take), b);
        pos += take;
    }
    REQUIRE(a.rows.size() == b.rows.size());
    for (size_t r = 0; r < a.rows.size(); ++r) {
        for (size_t i = 0; i < a.rows[r].size(); ++i) {
            CHECK(a.rows[r][i] == doctest::Approx(b.rows[r][i]).epsilon(1e-5));
        }
    }
}
