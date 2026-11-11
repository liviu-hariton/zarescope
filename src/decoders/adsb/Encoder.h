// SPDX-License-Identifier: GPL-3.0-or-later
//
// Encoder: builds DF17 extended squitters and modulates them into cu8 IQ at
// 2 MS/s, the reverse of ModeS.h and Demodulator.h.
//
// Zarescope only receives; nothing here is ever transmitted. The encoder
// exists so the decoder can be tested end to end on known messages, and so
// zarescope-synth can write a demo recording with aircraft in it.

#pragma once

#include "decoders/adsb/ModeS.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace zarescope::adsb {

RawMessage encodeIdentification(uint32_t icao, std::string_view callsign, int category = 3);
RawMessage encodeAirbornePosition(uint32_t icao, double latDeg, double lonDeg, int altitudeFt,
                                  bool odd);
RawMessage encodeVelocity(uint32_t icao, double groundSpeedKt, double trackDeg,
                          int verticalRateFpm);

// The CPR fields for a position (even or odd frame).
void cprEncode(double latDeg, double lonDeg, bool odd, uint32_t& latCpr, uint32_t& lonCpr);

// Appends the message as cu8 IQ at 2 MS/s: preamble then pulse-position
// bits, `amplitude` 0…1, on a carrier at 0 Hz with the given phase.
void modulate(const RawMessage& message, float amplitude, float phase, std::vector<uint8_t>& iq);

}  // namespace zarescope::adsb
