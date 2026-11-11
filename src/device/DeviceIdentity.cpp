// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/DeviceIdentity.h"

namespace zarescope::device {

DeviceIdentity identifyDevice(std::string_view manufacturer, std::string_view product,
                              TunerType tuner) {
    DeviceIdentity identity;
    identity.tuner = tuner;
    if (manufacturer != "RTLSDRBlog") {
        return identity;  // Other, Unverifiable
    }

    TunerType expected = TunerType::Unknown;
    if (product == "Blog V4") {
        identity.claimed = DongleModel::BlogV4;
        expected = TunerType::R828D;
    } else if (product == "Blog V4L") {
        identity.claimed = DongleModel::BlogV4Lite;
        expected = TunerType::R820T;
    } else if (product == "Blog V3") {
        identity.claimed = DongleModel::BlogV3;
        expected = TunerType::R820T;
    } else {
        return identity;
    }
    identity.verdict = tuner == expected ? IdentityVerdict::Consistent : IdentityVerdict::Mismatch;
    return identity;
}

std::string_view tunerName(TunerType tuner) {
    switch (tuner) {
    case TunerType::E4000: return "Elonics E4000";
    case TunerType::FC0012: return "Fitipower FC0012";
    case TunerType::FC0013: return "Fitipower FC0013";
    case TunerType::FC2580: return "FCI FC2580";
    case TunerType::R820T: return "Rafael Micro R820T/R860";
    case TunerType::R828D: return "Rafael Micro R828D";
    case TunerType::Unknown: break;
    }
    return "unknown tuner";
}

std::string_view modelName(DongleModel model) {
    switch (model) {
    case DongleModel::BlogV4: return "RTL-SDR Blog V4";
    case DongleModel::BlogV4Lite: return "RTL-SDR Blog V4 Lite";
    case DongleModel::BlogV3: return "RTL-SDR Blog V3";
    case DongleModel::Other: break;
    }
    return "";
}

}  // namespace zarescope::device
