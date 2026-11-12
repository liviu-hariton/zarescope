// SPDX-License-Identifier: GPL-3.0-or-later
//
// DiscoveryView: the Discover tab, where a new user starts (M6).
//
//   Start here   three steps, each ticked off when done: set the location,
//                connect a receiver, pick something below. The first step
//                not done yet gets the primary button.
//   categories   broadcast radio, aviation, amateurs and walkie-talkies,
//                weather satellites. Each entry says in two or three
//                sentences what you'll hear or see, which antenna helps,
//                and has one action (listen, show aircraft, plan passes,
//                or connect a receiver first).
//
// The texts live in the string table (EN and RO); the frequencies come
// from resources/presets.json through the preset id.

#pragma once

#include "ui/UiKit.h"

#include <optional>
#include <string>

namespace zarescope::ui {

struct DiscoveryState {
    bool hasStation = false;
    bool receiverRunning = false;  // a stick or a recording
    bool receiverIsDevice = false;
};

struct DiscoveryActions {
    bool openSettings = false;
    bool openDevices = false;
    std::optional<std::string> applyPreset;  // a preset id
    bool showAircraft = false;
    bool showSatellites = false;
};

class DiscoveryView {
public:
    DiscoveryActions draw(UiKit& ui, ImVec2 pos, ImVec2 size, const DiscoveryState& state);
};

}  // namespace zarescope::ui
