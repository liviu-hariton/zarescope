// SPDX-License-Identifier: GPL-3.0-or-later
//
// The macOS renderer: Dear ImGui on Metal, in a GLFW window.
//
// GLFW creates a plain Cocoa window with no graphics context (GLFW_NO_API);
// we give its content view a CAMetalLayer and draw ImGui into the layer's
// drawables. Follows ImGui's examples/example_glfw_metal/main.mm.
//
// Built with ARC, so the Objective-C members below are released with the
// C++ object.

#include "platform/Renderer.h"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <imgui_impl_metal.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

namespace zarescope::platform {

namespace {

class MetalRenderer final : public Renderer {
public:
    void setWindowHints() override { glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); }

    bool init(GLFWwindow* window, std::string& error) override {
        m_window = window;
        m_device = MTLCreateSystemDefaultDevice();
        if (m_device == nil) {
            error = "this Mac has no Metal device";
            return false;
        }
        m_queue = [m_device newCommandQueue];
        if (!ImGui_ImplMetal_Init(m_device)) {
            error = "Dear ImGui's Metal backend failed to start";
            return false;
        }
        m_initialised = true;

        NSWindow* nsWindow = glfwGetCocoaWindow(window);
        m_layer = [CAMetalLayer layer];
        m_layer.device = m_device;
        m_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        // Present in step with the display (no tearing, no busy rendering).
        m_layer.displaySyncEnabled = YES;
        nsWindow.contentView.layer = m_layer;
        nsWindow.contentView.wantsLayer = YES;

        m_pass = [MTLRenderPassDescriptor new];
        return true;
    }

    void renderFrame(const std::array<float, 4>& clearColor,
                     const std::function<void()>& buildFrame) override {
        @autoreleasepool {
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(m_window, &width, &height);
            if (width <= 0 || height <= 0) {
                return;  // minimised
            }
            m_layer.drawableSize = CGSizeMake(width, height);
            id<CAMetalDrawable> drawable = [m_layer nextDrawable];
            if (drawable == nil) {
                return;
            }

            id<MTLCommandBuffer> commands = [m_queue commandBuffer];
            m_pass.colorAttachments[0].clearColor =
                MTLClearColorMake(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
            m_pass.colorAttachments[0].texture = drawable.texture;
            m_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            m_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            id<MTLRenderCommandEncoder> encoder =
                [commands renderCommandEncoderWithDescriptor:m_pass];
            [encoder pushDebugGroup:@"Zarescope UI"];

            ImGui_ImplMetal_NewFrame(m_pass);
            buildFrame();
            ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commands, encoder);

            [encoder popDebugGroup];
            [encoder endEncoding];
            [commands presentDrawable:drawable];
            [commands commit];
        }
    }

    void shutdown() override {
        if (m_initialised) {
            ImGui_ImplMetal_Shutdown();
            m_initialised = false;
        }
    }

private:
    GLFWwindow* m_window = nullptr;
    id<MTLDevice> m_device = nil;
    id<MTLCommandQueue> m_queue = nil;
    CAMetalLayer* m_layer = nil;
    MTLRenderPassDescriptor* m_pass = nil;
    bool m_initialised = false;
};

}  // namespace

std::unique_ptr<Renderer> createRenderer() { return std::make_unique<MetalRenderer>(); }

}  // namespace zarescope::platform
