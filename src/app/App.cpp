// SPDX-License-Identifier: GPL-3.0-or-later
//
// Start-up, frame loop, shutdown. See App.h.

#include "app/App.h"

#include "app/PassAutomation.h"
#include "app/Settings.h"
#include "app/UpdateCheck.h"
#include "device/Presets.h"
#include "device/Receiver.h"
#include "net/HttpGet.h"
#include "platform/AppMenu.h"
#include "platform/AppPaths.h"
#include "platform/Desktop.h"
#include "platform/FileDialog.h"
#include "platform/Location.h"
#include "platform/Notifier.h"
#include "platform/Process.h"
#include "platform/Renderer.h"
#include "platform/WindowAppearance.h"
#include "sat/SatCatalog.h"
#include "sat/SatelliteService.h"
#include "store/Journal.h"
#include "ui/Guide.h"
#include "ui/MainWindow.h"
#include "ui/Typography.h"
#include "ui/theme/DesignTokens.h"
#include "ui/theme/ImGuiTheme.h"
#include "ui/theme/PlotTheme.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>  // the input queue, for applyInterfaceScale()
#include <implot.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
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
constexpr int kMinimumWidth = 1200;
constexpr int kMinimumHeight = 600;

// The loop sleeps between frames when nothing happens, so an idle window
// costs no CPU. It still wakes this often to tick the status bar clock.
constexpr double kIdleWakeSeconds = 0.25;
// After input, keep drawing at the display's rate this long, so hover
// highlights and tooltips (which need a few frames) show without delay.
constexpr auto kActiveFor = std::chrono::milliseconds(500);

// The Text size setting (M6+): the whole interface is drawn bigger by
// telling Dear ImGui the window is `scale` times smaller and its pixels
// `scale` times denser. Every size, gap and plot grows together, and the
// fonts are rasterized at the new density, so text stays sharp. Called
// between the GLFW backend's NewFrame (which sets the real size and queues
// mouse positions in window points) and ImGui::NewFrame (which reads them).
void applyInterfaceScale(float scale) {
    if (scale == 1.0F) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(io.DisplaySize.x / scale, io.DisplaySize.y / scale);
    io.DisplayFramebufferScale =
        ImVec2(io.DisplayFramebufferScale.x * scale, io.DisplayFramebufferScale.y * scale);
    // Mouse positions queued this frame are in window points: convert them.
    // (Input trickling is off, so the queue holds only this frame's events.)
    for (ImGuiInputEvent& event : ImGui::GetCurrentContext()->InputEventsQueue) {
        if (event.Type == ImGuiInputEventType_MousePos && event.MousePos.PosX != -FLT_MAX) {
            event.MousePos.PosX /= scale;
            event.MousePos.PosY /= scale;
        }
    }
}

// The window can't shrink below what the layout needs at this scale.
void applyWindowLimits(GLFWwindow* window, float scale) {
    const int minWidth = static_cast<int>(static_cast<float>(kMinimumWidth) * scale);
    const int minHeight = static_cast<int>(static_cast<float>(kMinimumHeight) * scale);
    glfwSetWindowSizeLimits(window, minWidth, minHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);
    if (width < minWidth || height < minHeight) {
        glfwSetWindowSize(window, std::max(width, minWidth), std::max(height, minHeight));
    }
}

// The user guide's Markdown, parsed; std::nullopt (and a note on stderr)
// when the file is missing: the Help tab then says so.
std::optional<ui::Guide> loadGuide(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "help: cannot read %s\n", file.c_str());
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return ui::parseGuide(text.str());
}

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
    std::unique_ptr<sat::SatelliteService> satellites;
    // Pass recording drives the receiver: it goes before the receiver does.
    std::unique_ptr<PassAutomation> automation;
    platform::KeepAwake keepAwake;
    // Settings and the journal write on their own threads; destroyed last,
    // so whatever the window saved on its way out is still written.
    std::unique_ptr<SettingsStore> settingsStore;
    std::unique_ptr<store::JournalService> journal;
    // The update check's worker; the window only reads its status.
    std::unique_ptr<UpdateChecker> updates;
    // Answers "use my current location"; goes before the window, whose
    // callback it may still call (with "cancelled").
    std::unique_ptr<platform::LocationFinder> locationFinder;
    // The menu bar, and the user guide in English and Romanian.
    std::unique_ptr<platform::AppMenu> menu;
    std::optional<ui::Guide> guideEnglish;
    std::optional<ui::Guide> guideRomanian;
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
        // Stop pass automation (and SatDump), then the receive threads:
        // nothing else may be drawing.
        automation.reset();
        if (receiver) {
            receiver.reset();
        }
        if (renderer) {
            renderer->shutdown();  // releases GPU textures, including the waterfall's
        }
        locationFinder.reset();  // a pending answer reaches the window first
        menu.reset();
        // The window's views unregister their textures: needs the context.
        mainWindow.reset();
        satellites.reset();  // joins its worker (a download in flight ends within its timeout)
        updates.reset();     // the same
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
    // The menu bar is ours (platform::AppMenu), not GLFW's default one.
    glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);
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
    // No imgui.ini next to the app: the window layout is fixed, and the
    // settings live in settings.json (M6).
    io.IniFilename = nullptr;
    // applyInterfaceScale() converts each frame's queued mouse positions;
    // trickling would carry some over to the next frame and convert them twice.
    io.ConfigInputTrickleEventQueue = false;

    platform::AppPaths paths = platform::appPaths();
    // ZARESCOPE_HOME (development): use that folder instead of Application
    // Support for settings, journal and data, so screenshots and tests never
    // touch the user's own (tools/capture-site-images.sh).
    if (const char* home = std::getenv("ZARESCOPE_HOME"); home != nullptr && home[0] != '\0') {
        paths.userData = home;
    }
    s.tokens = theme::DesignTokens::fromFile(paths.resources / "zarescope.tokens.json");

    std::string error;
    auto fonts = ui::loadFonts(*io.Fonts, paths.resources / "fonts", error);
    if (!fonts) {
        throw std::runtime_error(error);
    }
    s.typography.emplace(*fonts, *s.tokens);

    // Settings (M6): read once here, before the window shows anything.
    std::string warning;
    const std::filesystem::path settingsFile = paths.userData / "settings.json";
    const Settings settings = loadSettings(settingsFile, warning);
    if (!warning.empty()) {
        std::fprintf(stderr, "settings: %s\n", warning.c_str());
    }
    s.settingsStore = std::make_unique<SettingsStore>(settingsFile);
    // The data folder: the user's choice if it can be used, else the default.
    std::filesystem::path dataRoot = paths.userData;
    if (!settings.dataFolder.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(settings.dataFolder, ec);
        if (!ec && std::filesystem::is_directory(settings.dataFolder, ec)) {
            dataRoot = settings.dataFolder;
        } else {
            std::fprintf(stderr, "data folder %s isn't usable; using %s\n",
                         settings.dataFolder.c_str(), paths.userData.c_str());
        }
    }
    s.journal = std::make_unique<store::JournalService>(dataRoot / "journal.sqlite");

    // Every request names the app and its version, and nothing else.
    const std::string userAgent =
        std::string("Zarescope/") + ZARESCOPE_VERSION + " (+https://zarescope.eu)";
    // The update check (off unless turned on). ZARESCOPE_UPDATE_URL points it
    // at another file, a file:// one included, to try it out.
    {
        UpdateChecker::Options updates;
        updates.currentVersion = ZARESCOPE_VERSION;
        bool allowFile = false;
        if (const char* url = std::getenv("ZARESCOPE_UPDATE_URL");
            url != nullptr && url[0] != '\0') {
            updates.url = url;
            allowFile = true;
        }
        updates.automatic = settings.checkUpdates;
        updates.lastChecked = settings.updateCheckedUtc;
        updates.latestVersion = settings.latestVersion;
        updates.fetch = [userAgent, allowFile](const std::string& url, std::string& error) {
            return net::httpGet(url, userAgent, kMaxLatestBytes, error, allowFile);
        };
        s.updates = std::make_unique<UpdateChecker>(std::move(updates));
    }

    s.shell.variant = options.variant.value_or(settings.theme == "light"
                                                   ? theme::ThemeVariant::Light
                                                   : theme::ThemeVariant::Dark);
    s.shell.language = settings.language == "ro" ? ui::Language::Romanian : ui::Language::English;
    s.shell.textSize = settings.textSize;
    applyWindowLimits(s.window, interfaceScale(s.shell.textSize));
    applyThemeEverywhere(*s.tokens, s.shell.variant, s.window);

    s.receiver = std::make_unique<device::Receiver>();
    const std::filesystem::path recordings = dataRoot / "recordings";
    ui::MainWindowServices services;
    services.receiver = s.receiver.get();
    services.recordingsFolder = recordings;
    services.chooseRecording = [recordings] { return platform::chooseRecordingToOpen(recordings); };
    services.versionText = ZARESCOPE_VERSION;
    // Satellites: the catalog, the TLE cache, CelesTrak's identifiable User-Agent.
    if (auto catalog = sat::loadCatalog(paths.resources / "sat_catalog.json", error)) {
        // Pass recording (M5): recordings next to the user's, images in products/.
        AutomationConfig automation;
        automation.recordings = recordings;
        automation.products = dataRoot / "products";
        automation.keepBaseband = settings.keepBaseband;
        automation.ppm = settings.ppm;
        automation.satDumpCandidates = platform::satDumpCandidates();
        automation.software = std::string("Zarescope ") + ZARESCOPE_VERSION;
        s.automation = std::make_unique<PassAutomation>(*s.receiver, *catalog, automation);
        s.automation->setEnabled(settings.autoRecord);
        if (options.simulatePassSeconds) {
            // The first satellite automation can record, starting in 20 s.
            const sat::PassScheduler scheduler(*catalog);
            for (const sat::Satellite& satellite : *catalog) {
                sat::PlannedPass pass;
                pass.noradId = satellite.noradId;
                pass.satellite = satellite.name;
                pass.pass.aos = sat::Clock::now() + std::chrono::seconds(20);
                pass.pass.los = pass.pass.aos + std::chrono::seconds(*options.simulatePassSeconds);
                pass.pass.tca = pass.pass.aos + (pass.pass.los - pass.pass.aos) / 2;
                pass.pass.maxElevationDeg = 60.0;
                if (scheduler.planFor(pass)) {
                    s.automation->addSimulatedPass(pass);
                    s.automation->setEnabled(true);
                    break;
                }
            }
        }
        services.automation = s.automation.get();
        services.reveal = [](const std::filesystem::path& path) {
            platform::revealInFileManager(path);
        };
        services.keepAwake = [&keep = s.keepAwake](bool waiting, bool busy) {
            keep.set(busy      ? platform::KeepAwake::Level::Pass
                     : waiting ? platform::KeepAwake::Level::Waiting
                               : platform::KeepAwake::Level::Normal);
        };
        s.satellites = std::make_unique<sat::SatelliteService>(std::move(*catalog),
                                                               dataRoot / "tle_cache", userAgent);
        services.satellites = s.satellites.get();
    } else {
        std::fprintf(stderr, "%s\n", error.c_str());
    }
    services.notify = [](const std::string& title, const std::string& body) {
        platform::notify(title, body);
    };
    services.initialTab = options.tab;
    services.settings = settings;
    services.saveSettings = [&store = *s.settingsStore](const Settings& changed) {
        store.save(changed);
    };
    services.journal = s.journal.get();
    services.updates = s.updates.get();
    services.dataFolder = dataRoot;
    services.saveAutoRecord = !options.simulatePassSeconds.has_value();
    services.chooseExportFile = [](const std::string& name) {
        return platform::chooseExportFile(name);
    };
    services.chooseFolder = [dataRoot] { return platform::chooseFolder(dataRoot); };
    s.locationFinder = std::make_unique<platform::LocationFinder>();
    services.locate = [&finder = *s.locationFinder](
                          std::function<void(const ui::MainWindowServices::LocateResult&)> done) {
        finder.request([done = std::move(done)](const platform::LocationResult& found) {
            ui::MainWindowServices::LocateResult result;
            using From = platform::LocationResult::Error;
            using To = ui::MainWindowServices::LocateResult::Error;
            result.error = found.error == From::None     ? To::None
                           : found.error == From::Denied ? To::Denied
                           : found.error == From::Disabled ? To::Disabled
                                                           : To::Failed;
            result.latDeg = found.latDeg;
            result.lonDeg = found.lonDeg;
            result.altitudeM = found.altitudeM;
            result.accuracyM = found.accuracyM;
            result.detail = found.detail;
            done(result);
        });
    };
    if (options.station) {
        services.station = adsb::LatLon{options.station->first, options.station->second};
    }
    if (auto presets = device::loadPresets(paths.resources / "presets.json", error)) {
        services.presets = std::move(*presets);
    } else {
        std::fprintf(stderr, "%s\n", error.c_str());  // the app still works without presets
    }
    // The menu bar and its plumbing.
    s.menu = std::make_unique<platform::AppMenu>();
    services.installMenu = [&menu = *s.menu](const std::vector<platform::MenuSpec>& menus) {
        menu.install(menus);
    };
    services.setMenuState = [&menu = *s.menu](std::map<int, platform::MenuItemState> state) {
        menu.setState(std::move(state));
    };
    services.takeMenuCommands = [&menu = *s.menu] { return menu.takeCommands(); };
    services.openUrl = [](const std::string& url) { platform::openUrl(url); };
    services.quit = [window = s.window] { glfwSetWindowShouldClose(window, GLFW_TRUE); };
    // The user guide, read once here (resources/help in the bundle).
    s.guideEnglish = loadGuide(paths.resources / "help" / "guide.en.md");
    s.guideRomanian = loadGuide(paths.resources / "help" / "guide.ro.md");
    services.guideEnglish = s.guideEnglish ? &*s.guideEnglish : nullptr;
    services.guideRomanian = s.guideRomanian ? &*s.guideRomanian : nullptr;
    // About: the version, what it runs on, the copyright years.
    services.about.version = ZARESCOPE_VERSION;
    services.about.build = platform::systemVersion() + " · arm64 · Dear ImGui " IMGUI_VERSION;
    constexpr int kFirstYear = 2026;
    services.about.copyrightYears =
        ZARESCOPE_BUILD_YEAR > kFirstYear
            ? std::to_string(kFirstYear) + "–" + std::to_string(ZARESCOPE_BUILD_YEAR)
            : std::to_string(kFirstYear);
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
            applyInterfaceScale(interfaceScale(s.shell.textSize));
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
