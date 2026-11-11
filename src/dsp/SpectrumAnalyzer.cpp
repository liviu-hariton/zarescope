// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/SpectrumAnalyzer.h"

#include "dsp/IqConvert.h"
#include "dsp/Window.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace zarescope::dsp {

namespace {

// Floor for log10, so an all-zero bin reads −200 dBFS instead of −∞.
constexpr float kPowerFloor = 1e-20F;

}  // namespace

SpectrumAnalyzer::SpectrumAnalyzer(const SpectrumConfig& config)
    : m_config(config),
      m_fft(makeFftEngine(config.fftSize)),
      m_window(blackmanHarris(config.fftSize)),
      m_frame(config.fftSize),
      m_spectrum(config.fftSize),
      m_accumulated(config.fftSize, 0.0F),
      m_row(config.fftSize, 0.0F) {
    if (config.fftSize < 16) {
        throw std::invalid_argument("FFT size must be at least 16");
    }
    const float sum = windowSum(m_window);
    m_powerScale = 1.0F / (sum * sum);
    setSampleRate(m_sampleRateSps);
}

void SpectrumAnalyzer::setSampleRate(uint32_t sampleRateSps) {
    m_sampleRateSps = sampleRateSps == 0 ? 1 : sampleRateSps;
    if (m_config.framesPerRow > 0) {
        m_framesPerRow = m_config.framesPerRow;
    } else {
        const double framesPerSecond =
            static_cast<double>(m_sampleRateSps) / static_cast<double>(m_config.fftSize);
        m_framesPerRow = std::max<size_t>(
            1, static_cast<size_t>(std::lround(framesPerSecond / m_config.rowsPerSecond)));
    }
    reset();
}

void SpectrumAnalyzer::reset() {
    m_frameFill = 0;
    m_framesInRow = 0;
    std::fill(m_accumulated.begin(), m_accumulated.end(), 0.0F);
}

double SpectrumAnalyzer::binWidthHz() const {
    return static_cast<double>(m_sampleRateSps) / static_cast<double>(m_config.fftSize);
}

void SpectrumAnalyzer::process(std::span<const uint8_t> iq, SpectrumRowSink& sink) {
    const size_t samples = iq.size() / 2;
    size_t done = 0;
    while (done < samples) {
        const size_t take = std::min(samples - done, m_config.fftSize - m_frameFill);
        cu8ToComplex(iq.subspan(done * 2, take * 2), std::span(m_frame).subspan(m_frameFill, take));
        m_frameFill += take;
        done += take;
        if (m_frameFill == m_config.fftSize) {
            finishFrame(sink);
            m_frameFill = 0;
        }
    }
}

void SpectrumAnalyzer::finishFrame(SpectrumRowSink& sink) {
    const size_t n = m_config.fftSize;
    for (size_t i = 0; i < n; ++i) {
        m_frame[i] *= m_window[i];
    }
    m_fft->forward(m_frame, m_spectrum);
    for (size_t i = 0; i < n; ++i) {
        m_accumulated[i] += std::norm(m_spectrum[i]);
    }
    if (++m_framesInRow < m_framesPerRow) {
        return;
    }

    // Average, scale to dBFS and FFT-shift: bin k (0 = DC) goes to screen
    // index (k + N/2) mod N.
    const float scale = m_powerScale / static_cast<float>(m_framesInRow);
    const size_t half = n / 2;
    for (size_t k = 0; k < n; ++k) {
        const float power = std::max(m_accumulated[k] * scale, kPowerFloor);
        m_row[(k + half) % n] = 10.0F * std::log10(power);
    }
    std::fill(m_accumulated.begin(), m_accumulated.end(), 0.0F);
    m_framesInRow = 0;
    sink.onSpectrumRow(m_row);
}

}  // namespace zarescope::dsp
