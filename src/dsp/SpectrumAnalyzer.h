// SPDX-License-Identifier: GPL-3.0-or-later
//
// SpectrumAnalyzer: raw cu8 IQ in, power spectrum rows (dBFS) out.
//
//   cu8 bytes → complex float → Blackman-Harris window → FFT → |X|² →
//   average over `framesPerRow` frames → dBFS → one row
//
// - Rows are FFT-shifted: index 0 is −fs/2, index N/2 is the centre (DC),
//   index N−1 is just below +fs/2. That is the left-to-right order of the
//   spectrum and waterfall on screen.
// - Scale: a complex tone of amplitude A (full scale = 1) at a bin centre
//   reads 20·log10(A) dBFS. Noise per bin goes down as the FFT gets longer.
// - Consecutive FFT frames don't overlap; at the RTL-SDR's sample rates,
//   averaging many frames per row gives a steadier picture than overlapping.
// - After construction, process() never allocates.

#pragma once

#include "dsp/FftEngine.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace zarescope::dsp {

struct SpectrumConfig {
    size_t fftSize = 4096;
    // Rows per second the analyser aims for; sets framesPerRow from the sample
    // rate. The waterfall scrolls at this rate.
    double rowsPerSecond = 30.0;
    // When non-zero, average exactly this many FFT frames per row instead.
    size_t framesPerRow = 0;
};

// Receives finished rows. Called on the thread that calls process().
class SpectrumRowSink {
public:
    virtual ~SpectrumRowSink() = default;
    virtual void onSpectrumRow(std::span<const float> dbfs) = 0;
};

class SpectrumAnalyzer {
public:
    explicit SpectrumAnalyzer(const SpectrumConfig& config);

    // Sets the input sample rate, which decides frames per row. Discards a
    // partly accumulated row.
    void setSampleRate(uint32_t sampleRateSps);

    // Feeds raw cu8 bytes (I, Q, I, Q…; an odd trailing byte is ignored and
    // should not happen). Calls sink.onSpectrumRow() for every row completed.
    void process(std::span<const uint8_t> iq, SpectrumRowSink& sink);

    // Forgets partial frames and rows (after a retune, say).
    void reset();

    [[nodiscard]] size_t fftSize() const { return m_config.fftSize; }
    [[nodiscard]] size_t framesPerRow() const { return m_framesPerRow; }
    // Hz between neighbouring bins.
    [[nodiscard]] double binWidthHz() const;

private:
    void finishFrame(SpectrumRowSink& sink);

    SpectrumConfig m_config;
    uint32_t m_sampleRateSps = 2'400'000;
    size_t m_framesPerRow = 1;

    std::unique_ptr<FftEngine> m_fft;
    std::vector<float> m_window;
    float m_powerScale = 1.0F;  // 1 / (Σw)²

    std::vector<std::complex<float>> m_frame;     // time domain, windowed
    std::vector<std::complex<float>> m_spectrum;  // FFT output
    std::vector<float> m_accumulated;             // Σ|X|² per bin, this row
    std::vector<float> m_row;                     // the finished row, dBFS
    size_t m_frameFill = 0;                       // samples in m_frame
    size_t m_framesInRow = 0;
};

}  // namespace zarescope::dsp
