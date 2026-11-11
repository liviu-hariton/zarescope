// SPDX-License-Identifier: GPL-3.0-or-later
//
// resources/presets.json: the shipped file parses, and broken ones are
// refused with a reason.

#include "device/Presets.h"

#include <doctest/doctest.h>

#include <set>

using namespace zarescope::device;
using zarescope::dsp::DemodMode;

TEST_CASE("presets: the shipped file parses and covers the brief's list") {
    std::string error;
    const auto presets = loadPresets(ZARESCOPE_PRESETS_FILE, error);
    INFO(error);
    REQUIRE(presets);
    std::set<std::string> ids;
    for (const auto& preset : *presets) {
        ids.insert(preset.id);
        CHECK(preset.frequencyHz > 0);
    }
    // The M2 presets: FM radio, airband (AM), 2 m amateur, 446 MHz, ADS-B, Meteor.
    for (const char* id : {"fm-broadcast", "airband", "amateur-2m", "pmr446", "adsb", "meteor"}) {
        CAPTURE(id);
        CHECK(ids.contains(id));
    }
    const auto fm = std::find_if(presets->begin(), presets->end(),
                                 [](const Preset& p) { return p.id == "fm-broadcast"; });
    CHECK(fm->mode == DemodMode::Wfm);
    const auto air = std::find_if(presets->begin(), presets->end(),
                                  [](const Preset& p) { return p.id == "airband"; });
    CHECK(air->mode == DemodMode::Am);
}

TEST_CASE("presets: malformed files are refused") {
    std::string error;
    CHECK_FALSE(parsePresets("[", error));
    CHECK_FALSE(parsePresets(R"({"zarescopePresets":2,"presets":[]})", error));
    CHECK_FALSE(parsePresets(
        R"({"zarescopePresets":1,"presets":[{"id":"x","frequencyHz":1,"mode":"usb"}]})", error));
    CHECK(error.find("mode") != std::string::npos);
    CHECK_FALSE(parsePresets(R"({"zarescopePresets":1,"presets":[
        {"id":"x","frequencyHz":1,"mode":"am"},{"id":"x","frequencyHz":2,"mode":"am"}]})",
                             error));
    CHECK_FALSE(parsePresets(
        R"({"zarescopePresets":1,"presets":[{"id":"x","frequencyHz":-1,"mode":"am"}]})", error));
    CHECK(parseMode("wfm") == DemodMode::Wfm);
    CHECK_FALSE(parseMode("WFM"));
}
