// SPDX-License-Identifier: GPL-3.0-or-later
//
// Notifier: a system notification ("Meteor-M N2-4 pass in 5 minutes").
// On macOS through the UserNotifications framework: the first call asks
// the user for permission; if it's refused, notifications are skipped
// silently (the app shows the countdown anyway).

#pragma once

#include <string>

namespace zarescope::platform {

void notify(const std::string& title, const std::string& body);

}  // namespace zarescope::platform
