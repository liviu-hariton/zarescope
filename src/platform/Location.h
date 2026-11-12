// SPDX-License-Identifier: GPL-3.0-or-later
//
// Location: where this computer is, from the operating system, so the user
// doesn't have to type the station's coordinates (Settings › Station).
//
// On macOS through Core Location (Wi-Fi based, usually within tens of
// metres). The first request makes macOS ask the user for permission; the
// answer is remembered by the system. Nothing is sent anywhere by Zarescope.
//
// request() calls `done` exactly once, on the UI thread, while events are
// being processed: with a fix, or with why there is none (permission
// refused, Location Services off, no fix within 30 s, or the finder being
// destroyed first).

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace zarescope::platform {

struct LocationResult {
    enum class Error { None, Denied, Disabled, Failed };
    Error error = Error::Failed;
    double latDeg = 0.0;
    double lonDeg = 0.0;
    std::optional<double> altitudeM;  // above sea level, when the fix has one
    double accuracyM = 0.0;           // horizontal, radius
    std::string detail;               // for Failed: the system's reason
};

class LocationFinder {
public:
    LocationFinder();
    ~LocationFinder();  // a pending request ends as Failed ("cancelled")
    LocationFinder(const LocationFinder&) = delete;
    LocationFinder& operator=(const LocationFinder&) = delete;
    LocationFinder(LocationFinder&&) = delete;
    LocationFinder& operator=(LocationFinder&&) = delete;

    // UI thread. A request while one is pending fails at once ("busy").
    void request(std::function<void(const LocationResult&)> done);

    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
};

}  // namespace zarescope::platform
