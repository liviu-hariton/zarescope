// SPDX-License-Identifier: GPL-3.0-or-later
//
// AircraftStore: what is known about each aircraft heard, built from the
// decoded messages.
//
// Positions: an even and an odd frame less than 10 s apart decode globally;
// after that, single frames decode locally against the aircraft's last
// position (or against the station's location, before the first fix).
// A position more than 600 km from the previous one within a short time is
// rejected as a bad decode (a single-bit error the CRC let through).
//
// Aircraft not heard for 60 s are removed. Each keeps a short trail of
// positions for the radar.
//
// Single-threaded: the UI thread owns the store and feeds it the messages
// the DSP thread decoded.

#pragma once

#include "decoders/adsb/Cpr.h"
#include "decoders/adsb/ModeS.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace zarescope::adsb {

using Clock = std::chrono::system_clock;

struct TrailPoint {
    LatLon position;
    Clock::time_point time;
};

struct Aircraft {
    uint32_t icao = 0;
    std::string callsign;
    int category = 0;
    std::optional<int> altitudeFt;
    std::optional<double> groundSpeedKt;
    std::optional<double> trackDeg;
    std::optional<int> verticalRateFpm;
    std::optional<LatLon> position;
    Clock::time_point lastSeen;
    std::optional<Clock::time_point> lastPosition;
    uint64_t messages = 0;
    float signalDb = -100.0F;
    std::deque<TrailPoint> trail;  // oldest first

    // Raw CPR frames waiting for their partner.
    struct Frame {
        uint32_t lat = 0;
        uint32_t lon = 0;
        Clock::time_point time;
    };
    std::optional<Frame> even;
    std::optional<Frame> odd;
};

class AircraftStore {
public:
    static constexpr auto kExpiry = std::chrono::seconds(60);
    static constexpr auto kPairWindow = std::chrono::seconds(10);
    static constexpr size_t kTrailPoints = 120;

    // The station's location, for local decoding and distances. Optional:
    // global decoding works without it.
    void setStation(std::optional<LatLon> station) { m_station = station; }
    [[nodiscard]] const std::optional<LatLon>& station() const { return m_station; }

    // Feeds one valid message received at `now`.
    void update(const RawMessage& message, Clock::time_point now);
    // Removes aircraft silent for longer than kExpiry.
    void expire(Clock::time_point now);

    [[nodiscard]] const std::map<uint32_t, Aircraft>& aircraft() const { return m_aircraft; }
    [[nodiscard]] uint64_t messages() const { return m_messages; }

private:
    void updatePosition(Aircraft& aircraft, const AirbornePosition& position,
                        Clock::time_point now);

    std::map<uint32_t, Aircraft> m_aircraft;
    std::optional<LatLon> m_station;
    uint64_t m_messages = 0;
};

}  // namespace zarescope::adsb
