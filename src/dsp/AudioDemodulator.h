// SPDX-License-Identifier: GPL-3.0-or-later
//
// AudioDemodulator: one station out of the captured band, as audio.
//
//   cu8 → complex → shift the station to 0 Hz → low-pass + decimate to the
//   channel rate (a few stages) → squelch (channel power) → demodulate →
//   [FM broadcast: de-emphasis] → audio low-pass → resample to the sound
//   card's rate → volume
//
// Modes:
//   Wfm  wideband FM, broadcast radio: ±75 kHz deviation, 15 kHz audio,
//        50 µs de-emphasis in Europe (75 µs in the Americas)
//   Nfm  narrowband FM: amateur, PMR446, ±5 kHz deviation, voice audio
//   Am   amplitude modulation: the airband; envelope, DC removed, slow AGC
//
// configure() allocates (filters, buffers); process() never does. The
// offset, volume and squelch can change between blocks without
// reconfiguring.

#pragma once

#include "dsp/Filters.h"

#include <cstdint>
#include <span>
#include <vector>

namespace zarescope::dsp {

enum class DemodMode { Off, Wfm, Nfm, Am };

struct DemodSettings {
    DemodMode mode = DemodMode::Off;
    double offsetHz = 0.0;       // station frequency minus centre frequency
    double bandwidthHz = 0.0;    // channel width; 0 = the mode's default
    double deemphasisUs = 50.0;  // WFM only; 0 = off
    float squelchDb = -200.0F;   // channel power that opens the squelch, dBFS
    float volume = 0.7F;         // 0…1
};

// The mode's default channel width (Hz).
double defaultBandwidthHz(DemodMode mode);
// The range the UI offers for a mode's channel width.
double minBandwidthHz(DemodMode mode);
double maxBandwidthHz(DemodMode mode);

class AudioDemodulator {
public:
    // Prepares the chain for `inputRateSps` (the capture) and
    // `audioRateHz` (the sound card). Allocates.
    void configure(uint32_t inputRateSps, uint32_t audioRateHz, const DemodSettings& settings);

    // Cheap changes, between blocks.
    void setOffset(double offsetHz);
    void setVolume(float volume) { m_settings.volume = volume; }
    void setSquelch(float squelchDb) { m_settings.squelchDb = squelchDb; }

    // Demodulates one block of cu8 bytes and writes audio to `audio`, which
    // must hold maxAudioSamples(iq.size()) values. Returns the count written.
    size_t process(std::span<const uint8_t> iq, std::span<float> audio);

    // The most audio one block of `iqBytes` can produce.
    [[nodiscard]] size_t maxAudioSamples(size_t iqBytes) const;

    [[nodiscard]] const DemodSettings& settings() const { return m_settings; }
    [[nodiscard]] double channelRateHz() const { return m_channelRate; }
    // Channel power of the last block, dBFS, and whether the squelch let
    // the audio through.
    [[nodiscard]] float channelPowerDb() const { return m_channelPowerDb; }
    [[nodiscard]] bool squelchOpen() const { return m_squelchOpen; }

private:
    DemodSettings m_settings;
    uint32_t m_inputRate = 0;
    uint32_t m_audioRate = 48'000;
    double m_channelRate = 0.0;

    FrequencyShifter m_shifter;
    std::vector<DecimatingFir<Complex>> m_channelStages;
    DecimatingFir<float> m_audioFilter;
    Deemphasis m_deemphasis;
    DcBlocker m_dcBlocker;
    Resampler m_resampler;

    // Demodulator state.
    Complex m_previous{1.0F, 0.0F};
    float m_fmGain = 1.0F;
    float m_amLevel = 0.1F;  // slow AGC: running envelope average
    float m_gate = 0.0F;     // squelch gain, ramped to avoid clicks
    float m_channelPowerDb = -200.0F;
    bool m_squelchOpen = false;

    // Work buffers, sized in configure().
    std::vector<Complex> m_iq;
    std::vector<Complex> m_stageA;
    std::vector<Complex> m_stageB;
    std::vector<float> m_demod;
    std::vector<float> m_filtered;
};

}  // namespace zarescope::dsp
