// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zarescope's entry point.
//
// Command line (all optional; Finder passes none):
//   --theme dark|light   start in this theme
//                        (open build/Zarescope.app --args --theme light)

#include "app/App.h"

#include <cstdio>
#include <exception>
#include <string_view>

namespace {

// Returns false (and says why) on an argument it doesn't understand.
bool parseArguments(int argc, char** argv, zarescope::app::Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--theme" && i + 1 < argc) {
            const std::string_view value = argv[++i];
            if (value == "dark") {
                options.variant = zarescope::theme::ThemeVariant::Dark;
            } else if (value == "light") {
                options.variant = zarescope::theme::ThemeVariant::Light;
            } else {
                std::fprintf(stderr, "--theme takes dark or light, not %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return false;
            }
        } else if (arg.starts_with("-psn_")) {
            // Older macOS versions pass a process serial number; ignore it.
        } else {
            std::fprintf(stderr, "unknown argument: %.*s\n", static_cast<int>(arg.size()),
                         arg.data());
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    zarescope::app::Options options;
    if (!parseArguments(argc, argv, options)) {
        return 2;
    }
    try {
        zarescope::app::App app(options);
        return app.run();
    } catch (const std::exception& e) {
        // Start-up failures (missing token file or font, no Metal device).
        // Launched from Finder this goes to the system log; from a terminal,
        // `build/Zarescope.app/Contents/MacOS/Zarescope` shows it.
        std::fprintf(stderr, "Zarescope could not start: %s\n", e.what());
        return 1;
    }
}
