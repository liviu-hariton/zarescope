// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main window (design/docs/DESIGN.md §6), as far as M1 goes:
//
//   ┌ top bar (64) ────────────────────────────────────────────────────────┐
//   │ logo  98.000 000 MHz  [Gain auto ⌄]    [RTL-SDR Blog V4] (LIVE) [● Record] [⚙]        │
//   ├ banner (44): recording / an error, only when there is one ────────────┤
//   │ Spectrum                                         FFT 4096 · 586 Hz/bin │
//   │ spectrum (210)                                                         │
//   │ waterfall                                                              │
//   │ frequency axis                                                         │
//   ├ status bar (28) ──────────────────────────────────────────────────────┤
//   │ Zarescope 0.2.0  RTL-SDR Blog V4 · 2.4 MS/s       UTC 14:32:05  Local │
//   └───────────────────────────────────────────────────────────────────────┘
//
// With no source open, the body shows the empty state with two actions:
// connect a receiver, or open a recording.
//
// MainWindow never touches the device, the DSP or the disk itself: it
// posts commands to the Receiver and draws what the Receiver produced.

#pragma once

#include "app/PassAutomation.h"
#include "app/Settings.h"
#include "decoders/adsb/AircraftStore.h"
#include "device/Presets.h"
#include "device/Receiver.h"
#include "dsp/AudioDemodulator.h"
#include "sat/SatelliteService.h"
#include "store/Journal.h"
#include "ui/AircraftView.h"
#include "ui/DiscoveryView.h"
#include "ui/FrequencyDisplay.h"
#include "ui/JournalView.h"
#include "ui/SatelliteView.h"
#include "ui/SpectrumView.h"
#include "ui/Strings.h"
#include "ui/UiKit.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <map>
#include <set>
#include <string>

namespace zarescope::ui {

// What the user can change from the shell. The app re-applies the theme
// when `variant` changes.
struct ShellState {
    theme::ThemeVariant variant = theme::ThemeVariant::Dark;
    Language language = Language::English;
};

// What the window needs from the app and the platform.
struct MainWindowServices {
    device::Receiver* receiver = nullptr;
    std::filesystem::path recordingsFolder;
    // Shows the system's "open file" dialog for a .cu8 recording.
    std::function<std::optional<std::filesystem::path>()> chooseRecording;
    std::string versionText;
    std::vector<device::Preset> presets;
    // The station's location, if given on the command line (Settings can
    // change it; saving it arrives with M6).
    std::optional<adsb::LatLon> station;
    double stationAltitudeM = 0.0;
    // Satellites: TLEs and passes (M4); and a system notification.
    sat::SatelliteService* satellites = nullptr;
    std::function<void(const std::string& title, const std::string& body)> notify;
    std::string initialTab;  // "spectrum", "aircraft", "satellites" or empty
    // Automatic pass recording (M5); null without a satellite catalog.
    app::PassAutomation* automation = nullptr;
    // Shows a file or folder in Finder.
    std::function<void(const std::filesystem::path&)> reveal;
    // Tells the OS the app works unattended: `waiting` while automatic
    // recording is on, `busy` while a pass is recorded or decoded.
    std::function<void(bool waiting, bool busy)> keepAwake;
    // Settings (M6): what was saved last time, and how to save changes
    // (returns at once; the store writes on its own thread).
    app::Settings settings;
    std::function<void(const app::Settings&)> saveSettings;
    // The journal (M6); null when it couldn't be created.
    store::JournalService* journal = nullptr;
    // The system's "save as" dialog for an export, and its folder picker.
    std::function<std::optional<std::filesystem::path>(const std::string& defaultName)>
        chooseExportFile;
    std::function<std::optional<std::filesystem::path>()> chooseFolder;
    // Where the data actually lives this run (the default or the setting).
    std::filesystem::path dataFolder;
};

class MainWindow {
public:
    // Needs a live ImGui context (the spectrum view registers a texture).
    MainWindow(const theme::DesignTokens& tokens, const Typography& typography,
               MainWindowServices services);

    // Draws the whole window for one frame. Call between ImGui::NewFrame()
    // and ImGui::Render().
    void draw(ShellState& state);

    // True while something on screen changes by itself (live data, a
    // recording): the app then draws at the display's rate.
    [[nodiscard]] bool wantsContinuousFrames() const;

private:
    void pullData();
    void drawTopBar(ShellState& state);
    float drawBanner(float top);
    void drawBody();
    void drawEmptyState(ImVec2 origin, ImVec2 size);
    void drawSpectrumArea(ImVec2 origin, ImVec2 size);
    void drawAircraftArea(ImVec2 origin, ImVec2 size);
    void notifyUpcomingPasses();
    void updateAutomation();
    // Journal entries for what happened this frame (M6).
    void updateJournal();
    // Saves the settings when something changed this frame.
    void saveSettingsIfChanged(const ShellState& state);
    void drawJournalArea(ImVec2 origin, ImVec2 size);
    void drawDiscoveryArea(ImVec2 origin, ImVec2 size);
    void openDevice(uint32_t index);
    // The banner text for how a pass ended.
    [[nodiscard]] std::string outcomeMessage(const app::PassOutcome& outcome) const;
    // False while a pass holds the stick: tuning, gain, device and recording
    // are locked until it ends.
    [[nodiscard]] bool userMayControl() const;
    void applyStation();
    // The tab strip; returns its height.
    float drawTabs(ImVec2 origin, float width);
    void drawStatusBar();
    void drawDevicePopup();
    void drawGainPopup();
    void drawFftPopup();
    void drawAudioPopup();
    void drawPresetsPopup();
    void drawSettingsPopup(ShellState& state);
    void openRecording();
    void toggleRecording();
    // Moves the VFO; retunes the stick when the VFO leaves the captured span.
    void tuneTo(int64_t frequencyHz);
    void setMode(dsp::DemodMode mode);
    void applyPreset(const device::Preset& preset);
    [[nodiscard]] Vfo vfo() const;

    UiKit m_ui;
    MainWindowServices m_services;
    FrequencyDisplay m_frequency;
    SpectrumView m_spectrum;
    AircraftView m_aircraftView;
    adsb::AircraftStore m_aircraft;
    SatelliteView m_satelliteView;
    sat::SatelliteSnapshot m_satellites;
    std::set<std::pair<uint32_t, int64_t>> m_notified;  // passes already announced
    app::AutomationStatus m_automation;                  // this frame's
    uint64_t m_notifiedOutcome = 0;   // serial of the last pass outcome notified
    uint64_t m_dismissedOutcome = 0;  // … and of the last one dismissed
    // Settings: the current ones, and the last ones handed to the store.
    app::Settings m_settings;
    app::Settings m_savedSettings;
    // The journal: this frame's snapshot, its view, and what was written.
    store::JournalService::Snapshot m_journal;
    JournalView m_journalView;
    uint64_t m_journaledOutcome = 0;
    std::string m_journalDay;                          // aircraft rows are per UTC day
    std::map<uint32_t, std::string> m_journaledAircraft;  // ICAO → callsign written
    std::filesystem::path m_trackedRecording;          // the recording being watched
    bool m_trackedByUser = false;                      // … started by the user, not a pass
    bool m_trackedDone = false;
    DiscoveryView m_discoveryView;
    enum class Tab { Discover, Spectrum, Aircraft, Satellites, Journal };
    Tab m_tab = Tab::Spectrum;
    bool m_wasAdsbActive = false;
    // Station location being edited in Settings.
    double m_stationLat = 0.0;
    double m_stationLon = 0.0;
    double m_stationAlt = 0.0;
    float m_minElevation = 20.0F;

    // This frame's view of the receiver.
    device::ReceiverStatus m_status;
    device::RecorderStatus m_recording;
    device::SpectrumRow m_trace;

    // Listening: the VFO (absolute frequency) and the demodulator settings.
    dsp::DemodSettings m_demod;
    int64_t m_vfoHz = 0;
    std::string m_sourceKey;  // detects a newly opened source

    // Popups are opened from inside the bars but live at window level.
    bool m_openDevices = false;
    bool m_openAudio = false;
    bool m_openPresets = false;
    bool m_openSettings = false;
    ImVec2 m_audioAnchor;
    ImVec2 m_presetsAnchor;
    ImVec2 m_settingsAnchor;
    bool m_openGain = false;
    bool m_openFft = false;
    ImVec2 m_deviceAnchor;
    ImVec2 m_gainAnchor;
    ImVec2 m_fftAnchor;
    bool m_confirmBiasTee = false;
    float m_traceSmoothing = 0.4F;

    // Banners the user dismissed, so they don't come back for the same event.
    std::string m_dismissedError;
    std::filesystem::path m_dismissedRecording;
};

}  // namespace zarescope::ui
