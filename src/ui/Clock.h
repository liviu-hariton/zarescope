// SPDX-License-Identifier: GPL-3.0-or-later
//
// Clock text for the status bar. Zarescope keeps every time in UTC
// (std::chrono::system_clock); local time exists only on screen, so the
// conversion lives here, in the UI.

#pragma once

#include <chrono>
#include <string>

namespace zarescope::ui {

enum class ClockZone { Utc, Local };

// "HH:MM:SS", 24-hour, in UTC or in the Mac's time zone.
std::string formatClock(std::chrono::system_clock::time_point when, ClockZone zone);

}  // namespace zarescope::ui
