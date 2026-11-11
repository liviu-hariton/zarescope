// SPDX-License-Identifier: GPL-3.0-or-later

#include "decoders/adsb/Cpr.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zarescope::adsb {

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kCprScale = 131072.0;  // 2^17
constexpr int kZones = 15;              // NZ: latitude zones per hemisphere quarter
constexpr double kDLatEven = 360.0 / (4.0 * kZones);
constexpr double kDLatOdd = 360.0 / (4.0 * kZones - 1.0);
constexpr double kEarthRadiusKm = 6371.0;

// The modulo the CPR equations use: always non-negative.
double mod(double x, double y) { return x - y * std::floor(x / y); }

double radians(double deg) { return deg * kPi / 180.0; }

}  // namespace

int cprNl(double latDeg) {
    const double lat = std::fabs(latDeg);
    if (lat == 0.0) {
        return 59;
    }
    if (lat == 87.0) {
        return 2;
    }
    if (lat > 87.0) {
        return 1;
    }
    const double a = 1.0 - std::cos(kPi / (2.0 * kZones));
    const double b = std::pow(std::cos(radians(lat)), 2.0);
    return static_cast<int>(std::floor(2.0 * kPi / std::acos(1.0 - a / b)));
}

std::optional<LatLon> cprGlobal(uint32_t evenLat, uint32_t evenLon, uint32_t oddLat,
                                uint32_t oddLon, bool latestOdd) {
    const double lat0 = evenLat / kCprScale;
    const double lat1 = oddLat / kCprScale;
    const double lon0 = evenLon / kCprScale;
    const double lon1 = oddLon / kCprScale;

    const double j = std::floor(59.0 * lat0 - 60.0 * lat1 + 0.5);
    double latEven = kDLatEven * (mod(j, 60.0) + lat0);
    double latOdd = kDLatOdd * (mod(j, 59.0) + lat1);
    if (latEven >= 270.0) {
        latEven -= 360.0;
    }
    if (latOdd >= 270.0) {
        latOdd -= 360.0;
    }
    if (cprNl(latEven) != cprNl(latOdd)) {
        return std::nullopt;
    }
    const double lat = latestOdd ? latOdd : latEven;
    const int nl = cprNl(lat);
    const int ni = std::max(latestOdd ? nl - 1 : nl, 1);
    const double m = std::floor(lon0 * (nl - 1) - lon1 * nl + 0.5);
    double lon = (360.0 / ni) * (mod(m, ni) + (latestOdd ? lon1 : lon0));
    if (lon >= 180.0) {
        lon -= 360.0;
    }
    return LatLon{lat, lon};
}

LatLon cprLocal(uint32_t latCpr, uint32_t lonCpr, bool odd, const LatLon& reference) {
    const double latFraction = latCpr / kCprScale;
    const double lonFraction = lonCpr / kCprScale;
    const double dLat = odd ? kDLatOdd : kDLatEven;
    const double j = std::floor(reference.latDeg / dLat) +
                     std::floor(mod(reference.latDeg, dLat) / dLat - latFraction + 0.5);
    const double lat = dLat * (j + latFraction);
    const int ni = std::max(cprNl(lat) - (odd ? 1 : 0), 1);
    const double dLon = 360.0 / ni;
    const double m = std::floor(reference.lonDeg / dLon) +
                     std::floor(mod(reference.lonDeg, dLon) / dLon - lonFraction + 0.5);
    return {lat, dLon * (m + lonFraction)};
}

double distanceKm(const LatLon& from, const LatLon& to) {
    const double dLat = radians(to.latDeg - from.latDeg);
    const double dLon = radians(to.lonDeg - from.lonDeg);
    const double a = std::pow(std::sin(dLat / 2.0), 2.0) + std::cos(radians(from.latDeg)) *
                                                               std::cos(radians(to.latDeg)) *
                                                               std::pow(std::sin(dLon / 2.0), 2.0);
    return 2.0 * kEarthRadiusKm * std::asin(std::min(1.0, std::sqrt(a)));
}

double bearingDeg(const LatLon& from, const LatLon& to) {
    const double lat1 = radians(from.latDeg);
    const double lat2 = radians(to.latDeg);
    const double dLon = radians(to.lonDeg - from.lonDeg);
    const double y = std::sin(dLon) * std::cos(lat2);
    const double x =
        std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    const double bearing = std::atan2(y, x) * 180.0 / kPi;
    return bearing < 0.0 ? bearing + 360.0 : bearing;
}

}  // namespace zarescope::adsb
