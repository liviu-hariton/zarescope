// SPDX-License-Identifier: GPL-3.0-or-later

#include "sat/Orbit.h"

#include <CoordTopocentric.h>
#include <DateTime.h>
#include <Eci.h>
#include <Observer.h>
#include <SGP4.h>
#include <Tle.h>
#include <TleException.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zarescope::sat {

namespace {

using namespace std::chrono;

constexpr double kRadToDeg = 180.0 / std::numbers::pi;

// std::chrono UTC → libsgp4 DateTime (through calendar fields, so nothing
// depends on the library's internal epoch).
libsgp4::DateTime toDateTime(Clock::time_point when) {
    const auto day = floor<days>(when);
    const year_month_day date{day};
    const auto inDay = when - day;
    const auto h = duration_cast<hours>(inDay);
    const auto m = duration_cast<minutes>(inDay - h);
    const auto s = duration_cast<seconds>(inDay - h - m);
    const auto us = duration_cast<microseconds>(inDay - h - m - s);
    return {static_cast<int>(date.year()),
            static_cast<int>(static_cast<unsigned>(date.month())),
            static_cast<int>(static_cast<unsigned>(date.day())),
            static_cast<int>(h.count()),
            static_cast<int>(m.count()),
            static_cast<int>(s.count()),
            static_cast<int>(us.count())};
}

Clock::time_point fromDateTime(const libsgp4::DateTime& when) {
    const sys_days day = year{when.Year()} / when.Month() / when.Day();
    return day + hours{when.Hour()} + minutes{when.Minute()} + seconds{when.Second()} +
           microseconds{when.Microsecond()};
}

std::string trim(std::string_view text) {
    while (!text.empty() && (text.back() == '\r' || text.back() == ' ' || text.back() == '\n')) {
        text.remove_suffix(1);
    }
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    return std::string(text);
}

// TLE line checksum: digits count their value, '-' counts 1, mod 10.
bool checksumOk(const std::string& line) {
    if (line.size() < 69) {
        return false;
    }
    int sum = 0;
    for (size_t i = 0; i < 68; ++i) {
        const char c = line[i];
        if (c >= '0' && c <= '9') {
            sum += c - '0';
        } else if (c == '-') {
            sum += 1;
        }
    }
    return line[68] - '0' == sum % 10;
}

}  // namespace

std::optional<Tle> parseTle(std::string_view text, std::string& error) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string line = trim(text.substr(start, end - start));
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
        start = end + 1;
    }
    if (lines.size() < 2 || lines.size() > 3) {
        error = "a TLE has two lines, optionally after a name line";
        return std::nullopt;
    }
    Tle tle;
    if (lines.size() == 3) {
        tle.name = lines[0];
    }
    tle.line1 = lines[lines.size() - 2];
    tle.line2 = lines[lines.size() - 1];
    if (tle.line1.size() != 69 || tle.line2.size() != 69 || tle.line1[0] != '1' ||
        tle.line2[0] != '2') {
        error = "TLE lines must be 69 characters, starting with 1 and 2";
        return std::nullopt;
    }
    if (!checksumOk(tle.line1) || !checksumOk(tle.line2)) {
        error = "TLE checksum mismatch";
        return std::nullopt;
    }
    try {
        const libsgp4::Tle parsed(tle.name, tle.line1, tle.line2);
        tle.noradId = parsed.NoradNumber();
        tle.epoch = fromDateTime(parsed.Epoch());
    } catch (const libsgp4::TleException& e) {
        error = std::string("invalid TLE: ") + e.what();
        return std::nullopt;
    }
    return tle;
}

Propagator::Propagator(const Tle& tle)
    : m_tle(tle),
      m_libTle(std::make_unique<libsgp4::Tle>(tle.name, tle.line1, tle.line2)),
      m_sgp4(std::make_unique<libsgp4::SGP4>(*m_libTle)) {}

Propagator::~Propagator() = default;
Propagator::Propagator(Propagator&&) noexcept = default;
Propagator& Propagator::operator=(Propagator&&) noexcept = default;

void Propagator::temeState(Clock::time_point when, double position[3], double velocity[3]) const {
    const libsgp4::Eci eci = m_sgp4->FindPosition(toDateTime(when));
    const auto p = eci.Position();
    const auto v = eci.Velocity();
    position[0] = p.x;
    position[1] = p.y;
    position[2] = p.z;
    velocity[0] = v.x;
    velocity[1] = v.y;
    velocity[2] = v.z;
}

LookAngle Propagator::look(const Station& station, Clock::time_point when) const {
    libsgp4::Observer observer(station.latDeg, station.lonDeg, station.altitudeM / 1000.0);
    const libsgp4::Eci eci = m_sgp4->FindPosition(toDateTime(when));
    const libsgp4::CoordTopocentric topo = observer.GetLookAngle(eci);
    return {topo.azimuth * kRadToDeg, topo.elevation * kRadToDeg, topo.range, topo.rangeRate};
}

namespace {

double elevationAt(const Propagator& p, const Station& s, Clock::time_point t) {
    return p.look(s, t).elevationDeg;
}

// Narrows [low, high] (elevation changes sign across it) to the crossing,
// to within a second.
Clock::time_point bisectHorizon(const Propagator& p, const Station& s, Clock::time_point low,
                                Clock::time_point high) {
    const bool lowAbove = elevationAt(p, s, low) > 0.0;
    while (high - low > seconds{1}) {
        const auto mid = low + (high - low) / 2;
        if ((elevationAt(p, s, mid) > 0.0) == lowAbove) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return low + (high - low) / 2;
}

// The highest point between AOS and LOS (golden-section search; elevation
// has one maximum over a pass).
Clock::time_point findCulmination(const Propagator& p, const Station& s, Clock::time_point aos,
                                  Clock::time_point los) {
    constexpr double kInvPhi = 0.6180339887498949;
    double a = 0.0;
    double b = duration<double>(los - aos).count();
    auto at = [&](double t) { return aos + duration_cast<Clock::duration>(duration<double>(t)); };
    double c = b - kInvPhi * (b - a);
    double d = a + kInvPhi * (b - a);
    double fc = elevationAt(p, s, at(c));
    double fd = elevationAt(p, s, at(d));
    while (b - a > 0.5) {
        if (fc > fd) {
            b = d;
            d = c;
            fd = fc;
            c = b - kInvPhi * (b - a);
            fc = elevationAt(p, s, at(c));
        } else {
            a = c;
            c = d;
            fc = fd;
            d = a + kInvPhi * (b - a);
            fd = elevationAt(p, s, at(d));
        }
    }
    return at((a + b) / 2.0);
}

}  // namespace

std::vector<Pass> predictPasses(const Propagator& propagator, const Station& station,
                                Clock::time_point from, std::chrono::hours window,
                                double minElevationDeg) {
    // 30-s steps: a LEO pass lasts several minutes, so no pass is skipped;
    // each crossing is then refined by bisection.
    constexpr auto kStep = seconds{30};
    std::vector<Pass> passes;
    const auto end = from + window;

    auto t = from;
    // A pass in progress at `from`: walk back to its start.
    if (elevationAt(propagator, station, t) > 0.0) {
        auto back = t;
        while (elevationAt(propagator, station, back) > 0.0 && t - back < hours{1}) {
            back -= kStep;
        }
        t = back;
    }
    bool above = elevationAt(propagator, station, t) > 0.0;
    std::optional<Clock::time_point> aos;
    while (t < end || aos) {
        const auto next = t + kStep;
        const bool nextAbove = elevationAt(propagator, station, next) > 0.0;
        if (!above && nextAbove) {
            aos = bisectHorizon(propagator, station, t, next);
            if (*aos >= end) {
                break;
            }
        } else if (above && !nextAbove && aos) {
            const auto los = bisectHorizon(propagator, station, t, next);
            Pass pass;
            pass.aos = *aos;
            pass.los = los;
            pass.tca = findCulmination(propagator, station, pass.aos, pass.los);
            pass.maxElevationDeg = elevationAt(propagator, station, pass.tca);
            pass.aosAzimuthDeg = propagator.look(station, pass.aos).azimuthDeg;
            pass.losAzimuthDeg = propagator.look(station, pass.los).azimuthDeg;
            // Southern half of the sky = azimuth between 90° and 270°.
            const bool risesSouth = pass.aosAzimuthDeg > 90.0 && pass.aosAzimuthDeg < 270.0;
            pass.northbound = risesSouth;
            if (pass.maxElevationDeg >= minElevationDeg) {
                passes.push_back(pass);
            }
            aos.reset();
        }
        above = nextAbove;
        t = next;
    }
    return passes;
}

std::vector<LookAngle> passTrack(const Propagator& propagator, const Station& station,
                                 const Pass& pass, int steps) {
    std::vector<LookAngle> track;
    steps = std::max(steps, 1);
    const auto span = pass.los - pass.aos;
    for (int i = 0; i <= steps; ++i) {
        track.push_back(propagator.look(station, pass.aos + span * i / steps));
    }
    return track;
}

}  // namespace zarescope::sat
