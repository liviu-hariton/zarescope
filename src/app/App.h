// SPDX-License-Identifier: GPL-3.0-or-later
//
// App: start-up, the frame loop and shutdown.
//
// Start-up order matters: GLFW, the window (with the renderer's hints), the
// ImGui context, the design tokens and fonts, then the backends. Shutdown
// runs in reverse. A missing or broken token file or font stops start-up
// with a message, because a half-styled window would be worse than none.

#pragma once

#include "ui/theme/DesignTokens.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

namespace zarescope::app {

// Start-up choices from the command line (see main.cpp).
struct Options {
    // Start in this theme instead of the token file's default.
    std::optional<theme::ThemeVariant> variant;
    // Open this .cu8 recording at start-up.
    std::optional<std::filesystem::path> recording;
    // The station's location (latitude, longitude in degrees).
    std::optional<std::pair<double, double>> station;
};

class App {
public:
    explicit App(const Options& options);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    // Runs until the window closes. Returns the process exit code.
    int run();

private:
    struct State;
    std::unique_ptr<State> m_state;
};

}  // namespace zarescope::app
