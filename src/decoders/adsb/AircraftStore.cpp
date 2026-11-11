// SPDX-License-Identifier: GPL-3.0-or-later

#include "decoders/adsb/AircraftStore.h"

#include <cmath>

namespace zarescope::adsb {

namespace {

// Farther than this from the last fix, soon after it, is a bad decode.
constexpr double kMaxJumpKm = 600.0;
constexpr auto kJumpWindow = std::chrono::seconds(30);
// Local decoding is unambiguous within 180 NM of the reference.
constexpr double kLocalRangeKm = 333.0;

}  // namespace

void AircraftStore::update(const RawMessage& message, Clock::time_point now) {
    const std::span<const uint8_t> bytes(message.bytes.data(), message.lengthBytes);
    const int df = downlinkFormat(bytes);
    if (df != 11 && df != 17 && df != 18) {
        return;
    }
    const uint32_t icao = icaoAddress(bytes);
    Aircraft& aircraft = m_aircraft[icao];
    aircraft.icao = icao;
    aircraft.lastSeen = now;
    aircraft.signalDb = message.signalDb;
    ++aircraft.messages;
    ++m_messages;

    const auto squitter = decodeExtendedSquitter(bytes);
    if (!squitter) {
        return;  // DF11: we only learn that it's there
    }
    if (squitter->identification) {
        aircraft.callsign = squitter->identification->callsign;
        aircraft.category = squitter->identification->category;
    }
    if (squitter->position) {
        if (squitter->position->altitudeFt) {
            aircraft.altitudeFt = squitter->position->altitudeFt;
        }
        updatePosition(aircraft, *squitter->position, now);
    }
    if (squitter->velocity) {
        const Velocity& v = *squitter->velocity;
        if (v.groundSpeedKt) {
            aircraft.groundSpeedKt = v.groundSpeedKt;
            aircraft.trackDeg = v.trackDeg;
        } else if (v.airspeedKt) {
            aircraft.groundSpeedKt = v.airspeedKt;  // best available
            aircraft.trackDeg = v.headingDeg;
        }
        if (v.verticalRateFpm) {
            aircraft.verticalRateFpm = v.verticalRateFpm;
        }
    }
}

void AircraftStore::updatePosition(Aircraft& aircraft, const AirbornePosition& position,
                                   Clock::time_point now) {
    const Aircraft::Frame frame{position.latCpr, position.lonCpr, now};
    (position.odd ? aircraft.odd : aircraft.even) = frame;

    std::optional<LatLon> decoded;
    // A recent fix (or the station, before any) allows a local decode.
    const bool recentFix =
        aircraft.position && aircraft.lastPosition && now - *aircraft.lastPosition < kJumpWindow;
    if (recentFix) {
        decoded = cprLocal(position.latCpr, position.lonCpr, position.odd, *aircraft.position);
    } else if (aircraft.even && aircraft.odd &&
               (aircraft.even->time > aircraft.odd->time
                    ? aircraft.even->time - aircraft.odd->time
                    : aircraft.odd->time - aircraft.even->time) < kPairWindow) {
        decoded = cprGlobal(aircraft.even->lat, aircraft.even->lon, aircraft.odd->lat,
                            aircraft.odd->lon, position.odd);
    } else if (m_station) {
        const LatLon local = cprLocal(position.latCpr, position.lonCpr, position.odd, *m_station);
        if (distanceKm(*m_station, local) < kLocalRangeKm) {
            decoded = local;
        }
    }
    if (!decoded) {
        return;
    }
    // Sanity: no aircraft jumps hundreds of kilometres between fixes.
    if (recentFix && distanceKm(*aircraft.position, *decoded) > kMaxJumpKm) {
        return;
    }
    aircraft.position = decoded;
    aircraft.lastPosition = now;
    aircraft.trail.push_back({*decoded, now});
    while (aircraft.trail.size() > kTrailPoints) {
        aircraft.trail.pop_front();
    }
}

void AircraftStore::expire(Clock::time_point now) {
    std::erase_if(m_aircraft,
                  [now](const auto& entry) { return now - entry.second.lastSeen > kExpiry; });
}

}  // namespace zarescope::adsb
