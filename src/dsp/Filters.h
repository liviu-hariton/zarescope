// SPDX-License-Identifier: GPL-3.0-or-later
//
// Building blocks for the audio chain (M2): frequency shifting, FIR
// low-pass filters with decimation, de-emphasis, DC blocking and
// resampling. Each one keeps its own state, allocates only when it is
// configured, and processes blocks in place or into caller-owned buffers.
//
// References: R. G. Lyons, "Understanding Digital Signal Processing", 3rd
// ed. (windowed-sinc FIR design ch. 5, decimation ch. 10); the GNU Radio and
// SDR++ implementations of the same blocks were read for comparison, no code
// was copied.

#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace zarescope::dsp {

using Complex = std::complex<float>;

// Mixes a complex signal by −offset: a station `offsetHz` above the centre
// moves to 0 Hz. Uses a unit phasor rotated sample by sample (double
// precision), renormalised now and then so it never drifts in amplitude.
class FrequencyShifter {
public:
    void configure(double offsetHz, double sampleRateHz);
    void process(std::span<Complex> samples);

private:
    std::complex<double> m_phasor{1.0, 0.0};
    std::complex<double> m_step{1.0, 0.0};
    uint32_t m_sinceNormalise = 0;
};

// Low-pass FIR taps, windowed sinc (Blackman window), unity gain at DC.
// `cutoffHz` is the −6 dB point. `taps` is forced odd (linear phase,
// integer delay).
std::vector<float> designLowPass(double cutoffHz, double sampleRateHz, size_t taps);

// Taps needed for a transition band of `transitionHz` with a Blackman window
// (about 5.5 · fs / Δf), odd, clamped to [15, maxTaps].
size_t tapsForTransition(double transitionHz, double sampleRateHz, size_t maxTaps = 255);

// An FIR low-pass that keeps one output in `decimation`. Works on complex or
// real samples (T = Complex or float); the taps are real.
template <typename T>
class DecimatingFir {
public:
    DecimatingFir() = default;
    DecimatingFir(std::vector<float> taps, size_t decimation);

    // Reads `in`, writes at most in.size()/decimation + 1 outputs to `out`
    // (which must be that large), returns how many. Never allocates.
    size_t process(std::span<const T> in, std::span<T> out);
    [[nodiscard]] size_t decimation() const { return m_decimation; }

private:
    std::vector<float> m_taps;
    // Delay line stored twice in a row, so the newest `taps` samples are
    // always contiguous: m_history[m_pos … m_pos + taps).
    std::vector<T> m_history;
    size_t m_pos = 0;
    size_t m_decimation = 1;
    size_t m_phase = 0;  // inputs since the last output
};

extern template class DecimatingFir<Complex>;
extern template class DecimatingFir<float>;

// Single-pole low-pass that undoes the pre-emphasis of FM broadcasts:
// 50 µs in Europe, 75 µs in the Americas.
class Deemphasis {
public:
    void configure(double timeConstantUs, double sampleRateHz);
    void process(std::span<float> samples);

private:
    float m_alpha = 1.0F;
    float m_state = 0.0F;
};

// Removes the DC offset (AM carriers leave one after envelope detection).
class DcBlocker {
public:
    void process(std::span<float> samples);

private:
    float m_prevIn = 0.0F;
    float m_prevOut = 0.0F;
};

// Changes the sample rate by any ratio with cubic (Catmull-Rom)
// interpolation. The input must already be band-limited below the output's
// Nyquist frequency.
class Resampler {
public:
    void configure(double inputRateHz, double outputRateHz);
    // Writes up to out.size() samples, returns how many. Never allocates.
    size_t process(std::span<const float> in, std::span<float> out);

private:
    double m_step = 1.0;  // input samples per output sample
    double m_position = 0.0;
    float m_history[4] = {0.0F, 0.0F, 0.0F, 0.0F};  // the last four inputs
};

}  // namespace zarescope::dsp
