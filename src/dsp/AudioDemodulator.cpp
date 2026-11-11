// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/AudioDemodulator.h"

#include "dsp/IqConvert.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zarescope::dsp {

namespace {

// Peak deviation each FM mode is scaled for (full-scale audio at ±deviation).
constexpr double kWfmDeviationHz = 75'000.0;
constexpr double kNfmDeviationHz = 5'000.0;
// Audio bandwidth after demodulation.
constexpr double kWfmAudioHz = 15'000.0;
constexpr double kVoiceAudioHz = 4'000.0;
// The channel rate each mode aims for: enough for the channel width plus
// room for the filters' transition bands.
double channelRateTarget(DemodMode mode, double bandwidthHz) {
    switch (mode) {
    case DemodMode::Wfm: return std::max(200'000.0, bandwidthHz * 1.2);
    case DemodMode::Nfm:
    case DemodMode::Am: return std::max(24'000.0, bandwidthHz * 1.5);
    case DemodMode::Off: break;
    }
    return 48'000.0;
}

// Splits a total decimation into stages of at most 8, largest first, so
// each stage's filter stays short.
std::vector<size_t> decimationStages(size_t total) {
    std::vector<size_t> stages;
    while (total > 1) {
        size_t factor = 1;
        for (size_t f = 8; f >= 2; --f) {
            if (total % f == 0) {
                factor = f;
                break;
            }
        }
        if (factor == 1) {
            // A prime above 8: take it whole.
            factor = total;
        }
        stages.push_back(factor);
        total /= factor;
    }
    return stages;
}

}  // namespace

double defaultBandwidthHz(DemodMode mode) {
    switch (mode) {
    case DemodMode::Wfm: return 180'000.0;
    case DemodMode::Nfm: return 12'500.0;
    case DemodMode::Am: return 10'000.0;
    case DemodMode::Off: break;
    }
    return 0.0;
}

double minBandwidthHz(DemodMode mode) { return mode == DemodMode::Wfm ? 80'000.0 : 3'000.0; }

double maxBandwidthHz(DemodMode mode) { return mode == DemodMode::Wfm ? 250'000.0 : 25'000.0; }

void AudioDemodulator::configure(uint32_t inputRateSps, uint32_t audioRateHz,
                                 const DemodSettings& settings) {
    m_settings = settings;
    if (m_settings.bandwidthHz <= 0.0) {
        m_settings.bandwidthHz = defaultBandwidthHz(settings.mode);
    }
    m_inputRate = std::max<uint32_t>(inputRateSps, 1);
    m_audioRate = std::max<uint32_t>(audioRateHz, 1);
    m_channelStages.clear();
    if (m_settings.mode == DemodMode::Off) {
        m_channelRate = 0.0;
        return;
    }

    // Channel filtering and decimation, in stages.
    const double bandwidth = m_settings.bandwidthHz;
    const double target = channelRateTarget(m_settings.mode, bandwidth);
    const auto total = std::max<size_t>(1, static_cast<size_t>(std::floor(m_inputRate / target)));
    double rate = m_inputRate;
    for (size_t factor : decimationStages(total)) {
        const double outRate = rate / static_cast<double>(factor);
        const double cutoff = bandwidth / 2.0;
        // Nothing that would fold into the channel may pass: the stopband
        // starts at outRate − cutoff.
        const double transition = std::max(outRate - 2.0 * cutoff, outRate * 0.1);
        const size_t taps = tapsForTransition(transition, rate);
        m_channelStages.emplace_back(designLowPass(cutoff + transition / 2.0, rate, taps), factor);
        rate = outRate;
    }
    if (m_channelStages.empty()) {
        // Input already at channel rate: still limit to the channel.
        const size_t taps = tapsForTransition(rate * 0.1, rate);
        m_channelStages.emplace_back(designLowPass(bandwidth / 2.0, rate, taps), 1);
    }
    m_channelRate = rate;

    m_shifter = FrequencyShifter{};
    m_shifter.configure(m_settings.offsetHz, m_inputRate);

    // Demodulator and audio stages at the channel rate.
    const double deviation = m_settings.mode == DemodMode::Wfm ? kWfmDeviationHz : kNfmDeviationHz;
    m_fmGain = static_cast<float>(m_channelRate / (2.0 * std::numbers::pi * deviation));
    const double audioCutoff = m_settings.mode == DemodMode::Wfm
                                   ? kWfmAudioHz
                                   : std::min(kVoiceAudioHz, m_settings.bandwidthHz / 2.0);
    const double audioNyquist = std::min(m_channelRate, static_cast<double>(m_audioRate)) / 2.0;
    const double audioTransition = std::max(1'000.0, audioNyquist - audioCutoff);
    m_audioFilter = DecimatingFir<float>(
        designLowPass(std::min(audioCutoff + audioTransition / 4.0, audioNyquist * 0.95),
                      m_channelRate, tapsForTransition(audioTransition, m_channelRate, 127)),
        1);
    m_deemphasis = Deemphasis{};
    m_deemphasis.configure(m_settings.mode == DemodMode::Wfm ? m_settings.deemphasisUs : 0.0,
                           m_channelRate);
    m_dcBlocker = DcBlocker{};
    m_resampler = Resampler{};
    m_resampler.configure(m_channelRate, m_audioRate);
    m_previous = {1.0F, 0.0F};
    m_amLevel = 0.1F;
    m_gate = 0.0F;

    // Work buffers for the largest block a source produces (64 KiB of cu8).
    constexpr size_t kMaxBlockSamples = 65'536 / 2;
    m_iq.assign(kMaxBlockSamples, {});
    m_stageA.assign(kMaxBlockSamples + 1, {});
    m_stageB.assign(kMaxBlockSamples + 1, {});
    m_demod.assign(kMaxBlockSamples + 1, 0.0F);
    m_filtered.assign(kMaxBlockSamples + 1, 0.0F);
}

void AudioDemodulator::setOffset(double offsetHz) {
    m_settings.offsetHz = offsetHz;
    m_shifter.configure(offsetHz, m_inputRate);
}

size_t AudioDemodulator::maxAudioSamples(size_t iqBytes) const {
    if (m_settings.mode == DemodMode::Off || m_channelRate <= 0.0) {
        return 0;
    }
    const double channelSamples = static_cast<double>(iqBytes / 2) * m_channelRate / m_inputRate;
    return static_cast<size_t>(std::ceil((channelSamples + 2.0) * m_audioRate / m_channelRate)) + 4;
}

size_t AudioDemodulator::process(std::span<const uint8_t> iq, std::span<float> audio) {
    if (m_settings.mode == DemodMode::Off || m_channelStages.empty()) {
        return 0;
    }
    const size_t samples = std::min(iq.size() / 2, m_iq.size());
    std::span<Complex> work(m_iq.data(), samples);
    cu8ToComplex(iq.first(samples * 2), work);
    m_shifter.process(work);

    // Channel stages, ping-ponging between the two buffers.
    std::span<const Complex> in = work;
    std::vector<Complex>* target = &m_stageA;
    for (auto& stage : m_channelStages) {
        const size_t n = stage.process(in, *target);
        in = std::span<const Complex>(target->data(), n);
        target = target == &m_stageA ? &m_stageB : &m_stageA;
    }
    const std::span<const Complex> channel = in;

    // Squelch on the channel's mean power.
    double power = 0.0;
    for (const Complex& c : channel) {
        power += std::norm(c);
    }
    power = channel.empty() ? 0.0 : power / static_cast<double>(channel.size());
    m_channelPowerDb = static_cast<float>(10.0 * std::log10(std::max(power, 1e-20)));
    // 3 dB of hysteresis so a signal at the threshold doesn't chatter.
    const float threshold = m_settings.squelchDb - (m_squelchOpen ? 3.0F : 0.0F);
    m_squelchOpen = m_channelPowerDb >= threshold;

    // Demodulate.
    std::span<float> demod(m_demod.data(), channel.size());
    switch (m_settings.mode) {
    case DemodMode::Wfm:
    case DemodMode::Nfm:
        // Phase difference between consecutive samples = instantaneous
        // frequency (quadrature demodulation).
        for (size_t i = 0; i < channel.size(); ++i) {
            const Complex product = channel[i] * std::conj(m_previous);
            demod[i] = std::atan2(product.imag(), product.real()) * m_fmGain;
            m_previous = channel[i];
        }
        break;
    case DemodMode::Am:
        for (size_t i = 0; i < channel.size(); ++i) {
            demod[i] = std::abs(channel[i]);
        }
        m_dcBlocker.process(demod);
        // Slow AGC: follow the average level, scale it to about ±0.5.
        for (float& sample : demod) {
            m_amLevel += 0.0005F * (std::fabs(sample) - m_amLevel);
            sample *= 0.5F / std::max(m_amLevel, 1e-4F);
        }
        break;
    case DemodMode::Off: break;
    }
    m_deemphasis.process(demod);

    std::span<float> filtered(m_filtered.data(), m_filtered.size());
    const size_t filteredCount = m_audioFilter.process(demod, filtered);
    const size_t written = m_resampler.process(filtered.first(filteredCount), audio);

    // Volume and the squelch gate, ramped over ~5 ms so it never clicks.
    const float gateTarget = m_squelchOpen ? m_settings.volume : 0.0F;
    const float ramp = 1.0F / (0.005F * static_cast<float>(m_audioRate));
    for (size_t i = 0; i < written; ++i) {
        m_gate += std::clamp(gateTarget - m_gate, -ramp, ramp);
        audio[i] = std::clamp(audio[i] * m_gate, -1.0F, 1.0F);
    }
    return written;
}

}  // namespace zarescope::dsp
