// SPDX-License-Identifier: GPL-3.0-or-later
//
// Window functions for spectral analysis.

#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace zarescope::dsp {

// The 4-term Blackman-Harris window (sidelobes at about −92 dB), periodic
// form (denominator N, not N−1), which is the right one for FFT analysis.
// Coefficients from F. J. Harris, "On the use of windows for harmonic
// analysis with the discrete Fourier transform", Proc. IEEE, 1978.
std::vector<float> blackmanHarris(size_t length);

// Sum of the coefficients ("coherent gain" × N). A tone of amplitude A at a
// bin centre shows up as A × sum in that bin, so dividing by the sum makes a
// full-scale tone read 0 dBFS.
float windowSum(std::span<const float> window);

}  // namespace zarescope::dsp
