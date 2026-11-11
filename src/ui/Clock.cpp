// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/Clock.h"

#include <ctime>
#include <format>

namespace zarescope::ui {

std::string formatClock(std::chrono::system_clock::time_point when, ClockZone zone) {
    using namespace std::chrono;
    const auto secondsSinceEpoch = floor<seconds>(when);
    if (zone == ClockZone::Utc) {
        return std::format("{:%H:%M:%S}", secondsSinceEpoch);
    }
    // Local time. The C++20 time-zone database (std::chrono::zoned_time) is
    // missing from Apple's libc++, so this uses the C library, which every
    // platform has. std::localtime() returns a shared buffer, which is fine
    // because only the UI thread formats clocks.
    const std::time_t t = system_clock::to_time_t(secondsSinceEpoch);
    const std::tm* local = std::localtime(&t);
    if (local == nullptr) {
        return "--:--:--";
    }
    return std::format("{:02}:{:02}:{:02}", local->tm_hour, local->tm_min, local->tm_sec);
}

}  // namespace zarescope::ui
