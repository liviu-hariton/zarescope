// SPDX-License-Identifier: GPL-3.0-or-later
//
// WindowAppearance on macOS. Setting NSWindow.appearance overrides the
// system appearance for this window only: the title bar, the traffic-light
// buttons and any native sheets follow the app's theme. Everything inside the
// window is drawn by ImGui and already follows the theme.

#include "platform/WindowAppearance.h"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

namespace zarescope::platform {

void setWindowAppearance(GLFWwindow* window, Appearance appearance) {
    @autoreleasepool {
        NSWindow* nsWindow = glfwGetCocoaWindow(window);
        if (nsWindow == nil) {
            return;
        }
        nsWindow.appearance =
            [NSAppearance appearanceNamed:appearance == Appearance::Dark ? NSAppearanceNameDarkAqua
                                                                         : NSAppearanceNameAqua];
    }
}

}  // namespace zarescope::platform
