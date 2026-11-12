// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/Units.h"

#include "ui/Format.h"

#include <array>
#include <cmath>
#include <format>

namespace zarescope::ui {

namespace {

constexpr double kKmPerHourPerKnot = kMetresPerNauticalMile / 1000.0;
constexpr double kMilesPerHourPerKnot = kMetresPerNauticalMile / kMetresPerStatuteMile;

// Radar ranges: the smallest that holds every aircraft is used. Each list
// reaches about 400 km, past the line-of-sight limit for an aircraft at
// cruise height. The rings sit at quarters of the range, so every value
// divides by 4 and the ring labels stay whole numbers.
constexpr std::array kRangesKm{20.0, 40.0, 100.0, 200.0, 300.0, 400.0};
constexpr std::array kRangesNm{12.0, 20.0, 40.0, 100.0, 160.0, 220.0};
constexpr std::array kRangesMi{12.0, 20.0, 40.0, 100.0, 160.0, 240.0};

}  // namespace

UnitSystem unitSystemFromKey(std::string_view key) {
    if (key == "aviation") {
        return UnitSystem::Aviation;
    }
    if (key == "imperial") {
        return UnitSystem::Imperial;
    }
    return UnitSystem::Metric;
}

std::string_view unitSystemKey(UnitSystem system) {
    switch (system) {
    case UnitSystem::Aviation: return "aviation";
    case UnitSystem::Imperial: return "imperial";
    case UnitSystem::Metric: break;
    }
    return "metric";
}

Measurement formatAltitude(int altitudeFt, UnitSystem system) {
    if (system == UnitSystem::Metric) {
        const auto metres = static_cast<int>(std::lround(altitudeFt * kMetresPerFoot));
        return {formatThousands(metres), "m"};
    }
    return {formatThousands(altitudeFt), "ft"};
}

Measurement formatSpeed(double speedKt, UnitSystem system) {
    switch (system) {
    case UnitSystem::Metric: return {std::format("{:.0f}", speedKt * kKmPerHourPerKnot), "km/h"};
    case UnitSystem::Imperial:
        return {std::format("{:.0f}", speedKt * kMilesPerHourPerKnot), "mph"};
    case UnitSystem::Aviation: break;
    }
    return {std::format("{:.0f}", speedKt), "kt"};
}

Measurement formatDistance(double distanceKm, UnitSystem system) {
    return {std::format("{:.1f}", distanceKm / kmPerDistanceUnit(system)), distanceUnit(system)};
}

std::string_view distanceUnit(UnitSystem system) {
    switch (system) {
    case UnitSystem::Aviation: return "NM";
    case UnitSystem::Imperial: return "mi";
    case UnitSystem::Metric: break;
    }
    return "km";
}

double kmPerDistanceUnit(UnitSystem system) {
    switch (system) {
    case UnitSystem::Aviation: return kMetresPerNauticalMile / 1000.0;
    case UnitSystem::Imperial: return kMetresPerStatuteMile / 1000.0;
    case UnitSystem::Metric: break;
    }
    return 1.0;
}

std::span<const double> radarRanges(UnitSystem system) {
    switch (system) {
    case UnitSystem::Aviation: return kRangesNm;
    case UnitSystem::Imperial: return kRangesMi;
    case UnitSystem::Metric: break;
    }
    return kRangesKm;
}

std::string_view heightUnit(UnitSystem system) { return system == UnitSystem::Metric ? "m" : "ft"; }

double metresPerHeightUnit(UnitSystem system) {
    return system == UnitSystem::Metric ? 1.0 : kMetresPerFoot;
}

std::string unitSummary(UnitSystem system) {
    return std::format("{} · {} · {}", formatAltitude(0, system).unit,
                       formatSpeed(0.0, system).unit, distanceUnit(system));
}

}  // namespace zarescope::ui
