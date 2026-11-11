// SPDX-License-Identifier: GPL-3.0-or-later
//
// Compact Position Reporting (CPR): turning the 17-bit latitude and
// longitude of an airborne position into degrees.
//
// Two ways (Junzi Sun, "The 1090 MHz Riddle", ch. 5):
//   global  one even and one odd frame, received within 10 s, decode
//           unambiguously anywhere on Earth
//   local   one frame decodes relative to a reference within 180 NM (the
//           station's own position, or the aircraft's last known one)

#pragma once

#include <cstdint>
#include <optional>

namespace zarescope::adsb {

struct LatLon {
    double latDeg = 0.0;
    double lonDeg = 0.0;
};

// Number of longitude zones at a latitude (NL), 1…59.
int cprNl(double latDeg);

// Global decode from an even and an odd frame. `latestOdd` says which one
// arrived last; its position is returned. std::nullopt when the two frames
// lie in different longitude zones (the pair can't be combined).
std::optional<LatLon> cprGlobal(uint32_t evenLat, uint32_t evenLon, uint32_t oddLat,
                                uint32_t oddLon, bool latestOdd);

// Local decode of one frame relative to `reference`.
LatLon cprLocal(uint32_t latCpr, uint32_t lonCpr, bool odd, const LatLon& reference);

// Great-circle distance (km) and initial bearing (degrees, 0 = north) from
// `from` to `to`.
double distanceKm(const LatLon& from, const LatLon& to);
double bearingDeg(const LatLon& from, const LatLon& to);

}  // namespace zarescope::adsb
