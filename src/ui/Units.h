// SPDX-License-Identifier: GPL-3.0-or-later
//
// Units: how measurements are shown, chosen in Settings › Units. Pure
// functions, no ImGui. Only the display changes: decoders, settings.json
// and the journal keep their own units (altitudeFt, groundSpeedKt, km,
// altitudeM), and the conversion happens here, at the last step.
//
//   system     altitude   speed   distance   station altitude
//   metric     m          km/h    km         m
//   aviation   ft         kt      NM         ft
//   imperial   ft         mph     mi         ft
//
// Conversion factors are the exact definitions: 1 ft = 0.3048 m,
// 1 NM = 1852 m, 1 mi = 1609.344 m (international yard and pound, 1959).

#pragma once

#include <span>
#include <string>
#include <string_view>

namespace zarescope::ui {

enum class UnitSystem { Metric, Aviation, Imperial };

inline constexpr double kMetresPerFoot = 0.3048;
inline constexpr double kMetresPerNauticalMile = 1852.0;
inline constexpr double kMetresPerStatuteMile = 1609.344;

// The settings.json value ("metric", "aviation", "imperial") and back;
// anything unknown reads as metric.
UnitSystem unitSystemFromKey(std::string_view key);
std::string_view unitSystemKey(UnitSystem system);

// A value ready to draw: the number and its unit, kept apart because the
// unit renders in text/secondary (DESIGN.md §3).
struct Measurement {
    std::string value;
    std::string_view unit;
};

// An aircraft's altitude, from the feet ADS-B sends: "10 973 m", "36 000 ft".
Measurement formatAltitude(int altitudeFt, UnitSystem system);
// Ground speed from knots, whole units: "837 km/h", "452 kt", "520 mph".
Measurement formatSpeed(double speedKt, UnitSystem system);
// A distance from kilometres, one decimal: "18.4 km", "9.9 NM", "11.4 mi".
Measurement formatDistance(double distanceKm, UnitSystem system);

// Distances: the unit and how many kilometres one of it is.
std::string_view distanceUnit(UnitSystem system);
double kmPerDistanceUnit(UnitSystem system);
// Radar range choices in that unit, smallest first: round numbers in each.
std::span<const double> radarRanges(UnitSystem system);

// The station's altitude: metres or feet, and how many metres one of it is.
std::string_view heightUnit(UnitSystem system);
double metresPerHeightUnit(UnitSystem system);

// The three units of a system, for the line under the Settings choice:
// "m · km/h · km".
std::string unitSummary(UnitSystem system);

}  // namespace zarescope::ui
