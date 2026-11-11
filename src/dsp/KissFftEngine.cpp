// SPDX-License-Identifier: GPL-3.0-or-later
//
// FftEngine on KissFFT (BSD-3-Clause), single precision.

#include "dsp/FftEngine.h"

#include <kiss_fft.h>

#include <stdexcept>

namespace zarescope::dsp {

namespace {

// kiss_fft_cpx is {float r; float i;}, which has the same layout as
// std::complex<float> (the standard guarantees the array layout), so spans
// convert without copying.
static_assert(sizeof(kiss_fft_cpx) == sizeof(std::complex<float>));

class KissFftEngine final : public FftEngine {
public:
    explicit KissFftEngine(size_t size)
        : m_size(size), m_config(kiss_fft_alloc(static_cast<int>(size), 0, nullptr, nullptr)) {
        if (m_config == nullptr) {
            throw std::runtime_error("KissFFT could not allocate a plan");
        }
    }
    ~KissFftEngine() override { kiss_fft_free(m_config); }
    KissFftEngine(const KissFftEngine&) = delete;
    KissFftEngine& operator=(const KissFftEngine&) = delete;
    KissFftEngine(KissFftEngine&&) = delete;
    KissFftEngine& operator=(KissFftEngine&&) = delete;

    [[nodiscard]] size_t size() const override { return m_size; }

    void forward(std::span<const std::complex<float>> in,
                 std::span<std::complex<float>> out) override {
        kiss_fft(m_config, reinterpret_cast<const kiss_fft_cpx*>(in.data()),
                 reinterpret_cast<kiss_fft_cpx*>(out.data()));
    }

private:
    size_t m_size;
    kiss_fft_cfg m_config;
};

}  // namespace

std::unique_ptr<FftEngine> makeFftEngine(size_t size) {
    return std::make_unique<KissFftEngine>(size);
}

}  // namespace zarescope::dsp
