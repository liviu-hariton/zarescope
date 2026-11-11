// SPDX-License-Identifier: GPL-3.0-or-later
//
// Orbits and passes (M4).
//
//   Tle            two-line element set: what CelesTrak publishes for each
//                  satellite. Parsed and checksummed by libsgp4.
//   Propagator     SGP4/SDP4 (libsgp4, the Vallado 2006 implementation):
//                  where the satellite is at a time, and how it looks from
//                  the station (azimuth, elevation, range)
//   predictPasses  every pass over the station in a time window: AOS
//                  (rises above the horizon), TCA (highest point), LOS
//                  (sets), maximum elevation, azimuths, direction
//
// All times are UTC (std::chrono::system_clock).

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace libsgp4 {
class Tle;
class SGP4;
}  // namespace libsgp4

namespace zarescope::sat {

using Clock = std::chrono::system_clock;

// Where the station is: geodetic latitude and longitude in degrees (north
// and east positive), height above the WGS-84 ellipsoid in metres.
struct Station {
    double latDeg = 0.0;
    double lonDeg = 0.0;
    double altitudeM = 0.0;
};

struct Tle {
    std::string name;
    std::string line1;
    std::string line2;
    uint32_t noradId = 0;
    Clock::time_point epoch;
};

// Parses CelesTrak's "FORMAT=TLE" text: an optional name line followed by
// lines 1 and 2. Returns std::nullopt and fills `error` when the lines are
// malformed or their checksums fail.
std::optional<Tle> parseTle(std::string_view text, std::string& error);

struct LookAngle {
    double azimuthDeg = 0.0;    // 0 = north, clockwise
    double elevationDeg = 0.0;  // above the horizon
    double rangeKm = 0.0;
    double rangeRateKmS = 0.0;  // positive = moving away (Doppler sign)
};

class Propagator {
public:
    explicit Propagator(const Tle& tle);
    ~Propagator();
    Propagator(const Propagator&) = delete;
    Propagator& operator=(const Propagator&) = delete;
    Propagator(Propagator&&) noexcept;
    Propagator& operator=(Propagator&&) noexcept;

    // Position and velocity in the TEME frame (km, km/s). Throws when the
    // orbit has decayed.
    void temeState(Clock::time_point when, double position[3], double velocity[3]) const;

    // How the satellite looks from the station at `when`.
    [[nodiscard]] LookAngle look(const Station& station, Clock::time_point when) const;

    [[nodiscard]] const Tle& tle() const { return m_tle; }

private:
    Tle m_tle;
    std::unique_ptr<libsgp4::Tle> m_libTle;
    std::unique_ptr<libsgp4::SGP4> m_sgp4;
};

struct Pass {
    Clock::time_point aos;  // acquisition of signal: rises above 0°
    Clock::time_point tca;  // time of closest approach: highest elevation
    Clock::time_point los;  // loss of signal: sets below 0°
    double maxElevationDeg = 0.0;
    double aosAzimuthDeg = 0.0;
    double losAzimuthDeg = 0.0;
    bool northbound = false;  // rises in the south half, sets in the north half
};

// Passes whose AOS lies in [from, from + window) and that reach at least
// `minElevationDeg`, in time order. A pass already in progress at `from` is
// included, with its AOS in the past. The scan never runs more than a day
// past the window (an orbit that never sets yields no pass). SGP4 errors
// (a decayed orbit) propagate as exceptions: callers catch them.
std::vector<Pass> predictPasses(const Propagator& propagator, const Station& station,
                                Clock::time_point from, std::chrono::hours window,
                                double minElevationDeg);

// A pass's track over the sky, `steps` + 1 points from AOS to LOS, for the
// polar plot. Never throws: on an SGP4 error it returns the points so far.
std::vector<LookAngle> passTrack(const Propagator& propagator, const Station& station,
                                 const Pass& pass, int steps = 60);

}  // namespace zarescope::sat
