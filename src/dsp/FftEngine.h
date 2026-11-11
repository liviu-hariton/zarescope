// SPDX-License-Identifier: GPL-3.0-or-later
//
// FftEngine: a forward complex FFT of a fixed size.
//
// The interface exists so the KissFFT implementation (portable, used today)
// can be swapped for Apple Accelerate's vDSP later (v2) without touching the
// analysers that use it.

#pragma once

#include <complex>
#include <cstddef>
#include <memory>
#include <span>

namespace zarescope::dsp {

class FftEngine {
public:
    virtual ~FftEngine() = default;

    [[nodiscard]] virtual size_t size() const = 0;

    // Unnormalized forward DFT: out[k] = Σ in[n]·e^(−2πikn/N). Both spans hold
    // exactly size() values and must not overlap. Never allocates.
    virtual void forward(std::span<const std::complex<float>> in,
                         std::span<std::complex<float>> out) = 0;
};

// The default engine for `size` points (any size; powers of two are fastest).
std::unique_ptr<FftEngine> makeFftEngine(size_t size);

}  // namespace zarescope::dsp
