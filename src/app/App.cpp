// SPDX-License-Identifier: GPL-3.0-or-later
//
// Start-up, frame loop, shutdown. See App.h.

#include "app/App.h"

#include "device/Presets.h"
#include "device/Receiver.h"
#include "platform/AppPaths.h"
#include "platform/FileDialog.h"
#include "platform/Renderer.h"
#include "platform/WindowAppearance.h"
#include "ui/MainWindow.h"
#include "ui/Typography.h"
#include "ui/theme/DesignTokens.h"
#include "ui/theme/ImGuiTheme.h"
#include "ui/theme/PlotTheme.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <implot.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>

namespace zarescope::app {

namespace {

// Opening size, in points. DESIGN.md §6 draws the window at 1440×900; this
// fits a 13-inch MacBook Air's default resolution with room for the Dock.
constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 800;
// Below this the top bar's controls start to overlap.
constexpr int kMinimumWidth = 1120;
constexpr int kMinimumHeight = 600;

// The loop sleeps between frames when nothing happens, so an idle window
// costs no CPU. It still wakes this often to tick the status bar clock.
constexpr double kIdleWakeSeconds = 0.25;
// After input, keep drawing at the display's rate this long, so hover
// highlights and tooltips (which need a few frames) show without delay.
constexpr auto kActiveFor = std::chrono::milliseconds(500);

void glfwError(int code, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

std::array<float, 4> clearColor(const theme::DesignTokens& tokens, theme::ThemeVariant variant) {
    const auto rgba = tokens.color(variant, "bg/base").value_or(theme::Rgba{});
    return {rgba.r, rgba.g, rgba.b, 1.0F};
}

// True when the user is doing something this frame.
bool userActive() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.MouseDelta.x != 0.0F || io.MouseDelta.y != 0.0F || io.MouseWheel != 0.0F) {
        return true;
    }
    for (bool down : io.MouseDown) {
        if (down) {
            return true;
        }
    }
    return ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered() ||
           io.InputQueueCharacters.Size > 0 || ImGui::IsKeyDown(ImGuiKey_Tab) || io.NavActive;
}

platform::Appearance appearanceFor(theme::ThemeVariant variant) {
    return variant == theme::ThemeVariant::Dark ? platform::Appearance::Dark
                                                : platform::Appearance::Light;
}

// Applies one theme variant to everything drawn: ImGui, ImPlot and the
// native window chrome.
void applyThemeEverywhere(const theme::DesignTokens& tokens, theme::ThemeVariant variant,
                          GLFWwindow* window);

void reportTheme(const ui::ThemeReport& report) {
    for (const auto& name : report.unknownStyleFields) {
        std::fprintf(stderr, "theme: imgui.style.%s is not an ImGuiStyle field\n", name.c_str());
    }
    for (const auto& name : report.unknownColorSlots) {
        std::fprintf(stderr, "theme: imgui.colors.%s is not an ImGuiCol entry\n", name.c_str());
    }
    for (const auto& name : report.unmappedColorSlots) {
        std::fprintf(stderr, "theme: ImGuiCol_%s has no colour token\n", name.c_str());
    }
}

void applyThemeEverywhere(const theme::DesignTokens& tokens, theme::ThemeVariant variant,
                          GLFWwindow* window) {
    reportTheme(ui::applyTheme(tokens, variant, ImGui::GetStyle()));
    for (const auto& name : ui::applyPlotTheme(tokens, variant, ImPlot::GetStyle())) {
        std::fprintf(stderr, "theme: ImPlotCol_%s has no colour token\n", name.c_str());
    }
    platform::setWindowAppearance(window, appearanceFor(variant));
}

}  // namespace

// Everything the app owns. Its destructor tears down whatever start-up got
// to, in reverse order; it also runs when App's constructor throws halfway.
struct App::State {
    GLFWwindow* window = nullptr;
    std::unique_ptr<platform::Renderer> renderer;
    bool glfwStarted = false;
    bool imguiContext = false;
    bool implotContext = false;
    bool glfwBackend = false;
    // The receive chain. Declared before the window so the window, which
    // refers to it, goes first.
    std::unique_ptr<device::Receiver> receiver;
    std::optional<theme::DesignTokens> tokens;
    std::optional<ui::Typography> typography;
    std::optional<ui::MainWindow> mainWindow;
    ui::ShellState shell;

    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;

    ~State() {
        // Stop the receive threads first: nothing else may be drawing.
        if (receiver) {
            receiver.reset();
        }
        if (renderer) {
            renderer->shutdown();  // releases GPU textures, including the waterfall's
        }
        // The window's views unregister their textures: needs the context.
        mainWindow.reset();
        if (implotContext) {
            ImPlot::DestroyContext();
        }
        if (glfwBackend) {
            ImGui_ImplGlfw_Shutdown();
        }
        if (imguiContext) {
            ImGui::DestroyContext();
        }
        if (window != nullptr) {
            glfwDestroyWindow(window);
        }
        if (glfwStarted) {
            glfwTerminate();
        }
    }
};

App::App(const Options& options) : m_state(std::make_unique<State>()) {
    State& s = *m_state;

    glfwSetErrorCallback(glfwError);
    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error("GLFW failed to start");
    }
    s.glfwStarted = true;

    s.renderer = platform::createRenderer();
    s.renderer->setWindowHints();
    s.window = glfwCreateWindow(kInitialWidth, kInitialHeight, "Zarescope", nullptr, nullptr);
    if (s.window == nullptr) {
        throw std::runtime_error("cannot create the window");
    }
    glfwSetWindowSizeLimits(s.window, kMinimumWidth, kMinimumHeight, GLFW_DONT_CARE,
                            GLFW_DONT_CARE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    s.imguiContext = true;
    ImPlot::CreateContext();
    s.implotContext = true;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // No imgui.ini next to the app: window layout is fixed in M0. Settings
    // will live in Application Support (M6).
    io.IniFilename = nullptr;

    const platform::AppPaths paths = platform::appPaths();
    s.tokens = theme::DesignTokens::fromFile(paths.resources / "zarescope.tokens.json");

    std::string error;
    auto fonts = ui::loadFonts(*io.Fonts, paths.resources / "fonts", error);
    if (!fonts) {
        throw std::runtime_error(error);
    }
    s.typography.emplace(*fonts, *s.tokens);

    s.shell.variant = options.variant.value_or(s.tokens->defaultVariant());
    applyThemeEverywhere(*s.tokens, s.shell.variant, s.window);

    s.receiver = std::make_unique<device::Receiver>();
    const std::filesystem::path recordings = paths.userData / "recordings";
    ui::MainWindowServices services;
    services.receiver = s.receiver.get();
    services.recordingsFolder = recordings;
    services.chooseRecording = [recordings] { return platform::chooseRecordingToOpen(recordings); };
    services.versionText = ZARESCOPE_VERSION;
    if (options.station) {
        services.station = adsb::LatLon{options.station->first, options.station->second};
    }
    if (auto presets = device::loadPresets(paths.resources / "presets.json", error)) {
        services.presets = std::move(*presets);
    } else {
        std::fprintf(stderr, "%s\n", error.c_str());  // the app still works without presets
    }
    s.mainWindow.emplace(*s.tokens, *s.typography, std::move(services));
    if (options.recording) {
        s.receiver->openFile(*options.recording);
    }

    // Install GLFW callbacks (true): ImGui gets mouse, keyboard and focus.
    if (!ImGui_ImplGlfw_InitForOther(s.window, true)) {
        throw std::runtime_error("Dear ImGui's GLFW backend failed to start");
    }
    s.glfwBackend = true;
    if (!s.renderer->init(s.window, error)) {
        throw std::runtime_error(error);
    }
}

App::~App() = default;

int App::run() {
    State& s = *m_state;
    using Clock = std::chrono::steady_clock;
    auto lastActive = Clock::now();

    while (glfwWindowShouldClose(s.window) == GLFW_FALSE) {
        // Live data and recordings change by themselves: draw at the
        // display's rate. Otherwise sleep until input (or the clock tick).
        const bool recentlyActive = Clock::now() - lastActive < kActiveFor;
        if (recentlyActive || s.mainWindow->wantsContinuousFrames()) {
            glfwPollEvents();  // the display's refresh paces the loop
        } else {
            glfwWaitEventsTimeout(kIdleWakeSeconds);
        }

        const theme::ThemeVariant shownVariant = s.shell.variant;
        s.renderer->renderFrame(clearColor(*s.tokens, s.shell.variant), [&] {
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            if (userActive()) {
                lastActive = Clock::now();
            }
            s.mainWindow->draw(s.shell);
            ImGui::Render();
        });

        // The theme switch was used: swap the whole token set.
        if (s.shell.variant != shownVariant) {
            applyThemeEverywhere(*s.tokens, s.shell.variant, s.window);
            lastActive = Clock::now();
        }
    }
    return 0;
}

}  // namespace zarescope::app
