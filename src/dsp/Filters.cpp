// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/Filters.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

namespace zarescope::dsp {

namespace {
constexpr double kTwoPi = 2.0 * std::numbers::pi;
}  // namespace

// --- FrequencyShifter ---------------------------------------------------------

void FrequencyShifter::configure(double offsetHz, double sampleRateHz) {
    const double step = -kTwoPi * offsetHz / sampleRateHz;
    m_step = {std::cos(step), std::sin(step)};
}

void FrequencyShifter::process(std::span<Complex> samples) {
    for (Complex& sample : samples) {
        const std::complex<double> s(sample.real(), sample.imag());
        const std::complex<double> mixed = s * m_phasor;
        sample = {static_cast<float>(mixed.real()), static_cast<float>(mixed.imag())};
        m_phasor *= m_step;
        if (++m_sinceNormalise == 4096) {
            m_phasor /= std::abs(m_phasor);
            m_sinceNormalise = 0;
        }
    }
}

// --- FIR design ---------------------------------------------------------------

std::vector<float> designLowPass(double cutoffHz, double sampleRateHz, size_t taps) {
    taps |= 1U;  // odd
    std::vector<float> h(taps);
    const double fc = cutoffHz / sampleRateHz;  // cycles per sample
    const double mid = static_cast<double>(taps - 1) / 2.0;
    for (size_t i = 0; i < taps; ++i) {
        const double n = static_cast<double>(i) - mid;
        const double sinc =
            n == 0.0 ? 2.0 * fc : std::sin(kTwoPi * fc * n) / (std::numbers::pi * n);
        const double x = kTwoPi * static_cast<double>(i) / static_cast<double>(taps - 1);
        const double blackman = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
        h[i] = static_cast<float>(sinc * blackman);
    }
    const float sum = std::accumulate(h.begin(), h.end(), 0.0F);
    for (float& tap : h) {
        tap /= sum;
    }
    return h;
}

size_t tapsForTransition(double transitionHz, double sampleRateHz, size_t maxTaps) {
    const auto taps =
        static_cast<size_t>(std::ceil(5.5 * sampleRateHz / std::max(transitionHz, 1.0)));
    return std::clamp<size_t>(taps | 1U, 15, maxTaps | 1U);
}

// --- DecimatingFir --------------------------------------------------------------

template <typename T>
DecimatingFir<T>::DecimatingFir(std::vector<float> taps, size_t decimation)
    : m_taps(std::move(taps)),
      m_history(m_taps.size() * 2, T{}),
      m_decimation(std::max<size_t>(decimation, 1)) {
    // Reverse once so the dot product runs forward over the history.
    std::reverse(m_taps.begin(), m_taps.end());
}

template <typename T>
size_t DecimatingFir<T>::process(std::span<const T> in, std::span<T> out) {
    const size_t n = m_taps.size();
    size_t written = 0;
    for (const T& sample : in) {
        // Write the sample twice, so the window [m_pos, m_pos + n) is contiguous.
        m_history[m_pos] = sample;
        m_history[m_pos + n] = sample;
        m_pos = (m_pos + 1) % n;
        if (++m_phase < m_decimation) {
            continue;
        }
        m_phase = 0;
        if (written == out.size()) {
            continue;  // caller's buffer full: drop (sized correctly, this never happens)
        }
        T acc{};
        const T* window = &m_history[m_pos];
        for (size_t k = 0; k < n; ++k) {
            acc += window[k] * m_taps[k];
        }
        out[written++] = acc;
    }
    return written;
}

template class DecimatingFir<Complex>;
template class DecimatingFir<float>;

// --- Deemphasis, DcBlocker ------------------------------------------------------

void Deemphasis::configure(double timeConstantUs, double sampleRateHz) {
    if (timeConstantUs <= 0.0) {
        m_alpha = 1.0F;  // off
        return;
    }
    m_alpha = static_cast<float>(1.0 - std::exp(-1.0 / (sampleRateHz * timeConstantUs * 1e-6)));
}

void Deemphasis::process(std::span<float> samples) {
    for (float& sample : samples) {
        m_state += m_alpha * (sample - m_state);
        sample = m_state;
    }
}

void DcBlocker::process(std::span<float> samples) {
    constexpr float kPole = 0.995F;
    for (float& sample : samples) {
        const float out = sample - m_prevIn + kPole * m_prevOut;
        m_prevIn = sample;
        m_prevOut = out;
        sample = out;
    }
}

// --- Resampler -----------------------------------------------------------------

void Resampler::configure(double inputRateHz, double outputRateHz) {
    m_step = inputRateHz / outputRateHz;
    m_position = 0.0;
}

size_t Resampler::process(std::span<const float> in, std::span<float> out) {
    size_t written = 0;
    for (float sample : in) {
        // Shift the newest input in: history = [x(-2), x(-1), x(0), x(+1)].
        m_history[0] = m_history[1];
        m_history[1] = m_history[2];
        m_history[2] = m_history[3];
        m_history[3] = sample;
        // Emit every output whose position falls between x(0) and x(+1).
        while (m_position < 1.0 && written < out.size()) {
            const auto t = static_cast<float>(m_position);
            const float p0 = m_history[0];
            const float p1 = m_history[1];
            const float p2 = m_history[2];
            const float p3 = m_history[3];
            // Catmull-Rom spline between p1 and p2.
            out[written++] = 0.5F * (2.0F * p1 + (-p0 + p2) * t +
                                     (2.0F * p0 - 5.0F * p1 + 4.0F * p2 - p3) * t * t +
                                     (-p0 + 3.0F * p1 - 3.0F * p2 + p3) * t * t * t);
            m_position += m_step;
        }
        m_position -= 1.0;
    }
    return written;
}

}  // namespace zarescope::dsp
