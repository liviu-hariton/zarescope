// SPDX-License-Identifier: GPL-3.0-or-later
//
// AppPaths: where the app's own files and the user's data live on this
// operating system.

#pragma once

#include <filesystem>

namespace zarescope::platform {

struct AppPaths {
    // Read-only files shipped with the app: the design tokens, fonts, and
    // later the satellite catalog and presets. On macOS:
    // Zarescope.app/Contents/Resources.
    std::filesystem::path resources;

    // The user's settings, TLE cache, recordings, decoded images and journal.
    // On macOS: ~/Library/Application Support/Zarescope. Not created here;
    // whoever writes there first creates it.
    std::filesystem::path userData;
};

AppPaths appPaths();

}  // namespace zarescope::platform
