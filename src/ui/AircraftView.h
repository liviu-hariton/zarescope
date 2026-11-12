// SPDX-License-Identifier: GPL-3.0-or-later
//
// AircraftView: the Aircraft tab (M3). A table on the left and a radar on
// the right, centred on the station (design/docs/DESIGN.md §5, aircraft row).
//
//   table  heading arrow, callsign (or the ICAO address), altitude, speed,
//          distance, seconds since last heard. Numbers are
//          right-aligned mono, units in text/secondary; rows not heard for
//          30 s are dimmed; the selected row uses accent/soft
//   radar  north up, range rings that grow with the farthest aircraft,
//          short trails, arrows along each track, callsign and altitude
//
// Selecting an aircraft in either highlights it in both. Altitude, speed and
// distance are in the units chosen in Settings (ui/Units.h).

#pragma once

#include "decoders/adsb/AircraftStore.h"
#include "ui/UiKit.h"
#include "ui/Units.h"

#include <cstdint>
#include <optional>

namespace zarescope::ui {

class AircraftView {
public:
    // Draws into [pos, pos + size].
    void draw(UiKit& ui, ImVec2 pos, ImVec2 size, const adsb::AircraftStore& store,
              UnitSystem units);

private:
    void drawTable(UiKit& ui, ImVec2 pos, ImVec2 size, const adsb::AircraftStore& store,
                   UnitSystem units);
    void drawRadar(UiKit& ui, ImVec2 pos, ImVec2 size, const adsb::AircraftStore& store,
                   UnitSystem units);

    std::optional<uint32_t> m_selected;
};

}  // namespace zarescope::ui
