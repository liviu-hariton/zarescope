// SPDX-License-Identifier: GPL-3.0-or-later
//
// DeviceIdentity: what an RTL-SDR stick says it is, and whether its tuner
// agrees.
//
// RTL-SDR Blog sticks write their model into the USB EEPROM strings
// (manufacturer "RTLSDRBlog", product "Blog V4" / "Blog V4L" / "Blog V3").
// librtlsdr uses exactly those strings to switch on V4-specific behaviour,
// so we compare them the same way (exact match). The tuner chip is read
// from the hardware over I²C and is much harder to fake:
//
//   Blog V4    must report an R828D tuner
//   Blog V4L   reports R820T (the R828S answers like an R820T)
//   Blog V3    reports R820T (the R820T2)
//
// A mismatch means the EEPROM claims a model the hardware isn't: the usual
// sign of a counterfeit. A match is necessary, not sufficient: a clone with
// the same tuner can copy the strings too. Further checks need a signal
// (for a V4: HF reception below 24 MHz without direct sampling).

#pragma once

#include <string_view>

namespace zarescope::device {

// Mirrors librtlsdr's enum rtlsdr_tuner.
enum class TunerType { Unknown, E4000, FC0012, FC0013, FC2580, R820T, R828D };

// The model the EEPROM claims.
enum class DongleModel { BlogV4, BlogV4Lite, BlogV3, Other };

enum class IdentityVerdict {
    Consistent,    // the tuner is the one this model uses
    Mismatch,      // the EEPROM claims a model whose tuner this isn't
    Unverifiable,  // no model claim to check (generic stick)
};

struct DeviceIdentity {
    DongleModel claimed = DongleModel::Other;
    TunerType tuner = TunerType::Unknown;
    IdentityVerdict verdict = IdentityVerdict::Unverifiable;
};

DeviceIdentity identifyDevice(std::string_view manufacturer, std::string_view product,
                              TunerType tuner);

// The chip's name, e.g. "Rafael Micro R828D". Technical names: not translated.
std::string_view tunerName(TunerType tuner);

// The model's name, e.g. "RTL-SDR Blog V4". For Other, an empty string.
std::string_view modelName(DongleModel model);

}  // namespace zarescope::device
