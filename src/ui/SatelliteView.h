// SPDX-License-Identifier: GPL-3.0-or-later
//
// SatelliteView: the Satellites tab (M4, DESIGN.md §5).
//
//   next-pass card   "NEXT PASS" caption in pass-imminent violet, the
//                    satellite, frequency and mode, a T−MM:SS countdown, a
//                    progress bar for the last 15 minutes before AOS, and
//                    three tiles: max elevation, AOS (time · azimuth), LOS
//   pass list        the next 48 hours: satellite, start (local time), max
//                    elevation, duration, direction
//   polar plot       the selected pass over the sky: rings at 0°, 30°, 60°,
//                    north up, east right; the track solid where flown and
//                    dotted where predicted; the satellite while it's up
//   TLE line         where the orbits came from and how old they are
//   automation       (M5) above the list: the "record and decode passes
//                    automatically" switch, what it does, and whether
//                    SatDump is installed (with the install command if not)

#pragma once

#include "app/PassAutomation.h"
#include "sat/Orbit.h"
#include "sat/SatelliteService.h"
#include "ui/UiKit.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zarescope::ui {

// What the user asked for in the Satellites tab this frame.
struct SatelliteViewActions {
    std::optional<bool> autoRecord;  // the switch was flipped to this
    bool checkSatDump = false;
};

class SatelliteView {
public:
    // `automation` is null when the app runs without pass automation.
    SatelliteViewActions draw(UiKit& ui, ImVec2 pos, ImVec2 size,
                              const sat::SatelliteSnapshot& snapshot,
                              const std::vector<sat::Satellite>& catalog,
                              const app::AutomationStatus* automation);

private:
    // Returns the height used.
    float drawAutomation(UiKit& ui, ImVec2 pos, float width, const app::AutomationStatus& status,
                         SatelliteViewActions& actions);
    void drawCard(UiKit& ui, ImVec2 pos, float width, const sat::PlannedPass& next,
                  const std::vector<sat::Satellite>& catalog, bool willRecord);
    void drawPassList(UiKit& ui, ImVec2 pos, ImVec2 size, const sat::SatelliteSnapshot& snapshot);
    void drawPolar(UiKit& ui, ImVec2 pos, ImVec2 size, const sat::SatelliteSnapshot& snapshot);

    // The selected pass (by satellite and AOS), and the propagator and
    // track computed for it.
    std::optional<std::pair<uint32_t, sat::Clock::time_point>> m_selected;
    std::unique_ptr<sat::Propagator> m_propagator;
    std::string m_propagatorLine1;
    std::vector<sat::LookAngle> m_track;
    std::optional<std::pair<uint32_t, sat::Clock::time_point>> m_trackFor;
    float m_cardHeight = 0.0F;  // measured when the card is drawn
};

// "T−04:32", or "T−1:04:32" from an hour; "T+…" once the time has passed.
std::string formatCountdown(std::chrono::seconds remaining);

}  // namespace zarescope::ui
