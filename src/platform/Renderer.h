// SPDX-License-Identifier: GPL-3.0-or-later
//
// Renderer: draws Dear ImGui's output into the window with the operating
// system's graphics API. macOS uses Metal (src/platform/macos); a Linux or
// Windows port adds its own implementation of this interface (Vulkan or
// OpenGL) and nothing else in the app changes.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>

struct GLFWwindow;

namespace zarescope::platform {

class Renderer {
public:
    virtual ~Renderer() = default;

    // Sets the GLFW window hints this graphics API needs. Call after
    // glfwInit() and before glfwCreateWindow().
    virtual void setWindowHints() = 0;

    // Attaches to the window and starts ImGui's renderer backend. Call after
    // ImGui::CreateContext(). On failure, returns false and fills `error`.
    virtual bool init(GLFWwindow* window, std::string& error) = 0;

    // Renders one frame. The renderer prepares its backend, then calls
    // `buildFrame`, which must run the whole ImGui frame: platform backend
    // NewFrame, ImGui::NewFrame(), the UI, ImGui::Render(). The renderer then
    // draws ImGui::GetDrawData() over `clearColor` (RGBA, 0..1) and presents.
    // When the window can't be drawn to (minimised), the frame is skipped and
    // `buildFrame` isn't called.
    virtual void renderFrame(const std::array<float, 4>& clearColor,
                             const std::function<void()>& buildFrame) = 0;

    // Stops the backend. Call before ImGui::DestroyContext().
    virtual void shutdown() = 0;
};

// The renderer for the platform the app was built for.
std::unique_ptr<Renderer> createRenderer();

}  // namespace zarescope::platform
