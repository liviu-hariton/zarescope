// SPDX-License-Identifier: GPL-3.0-or-later
//
// WindowAppearance: makes the parts of the window the operating system draws
// (on macOS: the title bar and its buttons) follow the app's theme instead
// of the system-wide light/dark setting.

#pragma once

struct GLFWwindow;

namespace zarescope::platform {

enum class Appearance { Dark, Light };

// Call after the window exists, and again whenever the theme changes.
void setWindowAppearance(GLFWwindow* window, Appearance appearance);

}  // namespace zarescope::platform
