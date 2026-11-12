// SPDX-License-Identifier: GPL-3.0-or-later
//
// Settings: what the user chose, kept between runs in
// ~/Library/Application Support/Zarescope/settings.json (docs/FORMATS.md).
//
//   parseSettings / settingsToJson   pure, tested; unknown keys are ignored
//                                    and bad values fall back to defaults,
//                                    so an old or hand-edited file never
//                                    stops the app from starting
//   SettingsStore                    writes on its own thread (the UI
//                                    thread never touches the disk): save()
//                                    keeps only the latest copy, and the
//                                    file is replaced atomically (written
//                                    to a temporary file, then renamed)

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace zarescope::app {

struct StationSetting {
    double latDeg = 0.0;
    double lonDeg = 0.0;
    double altitudeM = 0.0;
    bool operator==(const StationSetting&) const = default;
};

struct Settings {
    std::string theme = "dark";   // "dark" or "light"
    std::string language = "en";  // "en" or "ro"
    // How big the whole interface is drawn: "standard", "large", "larger".
    std::string textSize = "standard";
    std::optional<StationSetting> station;
    double minElevationDeg = 20.0;
    bool autoRecord = false;                  // start the pass scheduler with the app
    bool keepBaseband = false;                // keep pass recordings after a decode
    uint32_t defaultSampleRateSps = 2'400'000;  // when a stick is opened
    int ppm = 0;                              // frequency correction for the stick
    std::string dataFolder;  // empty = the default (Application Support/Zarescope)
    bool discoverySeen = false;  // the first run opens on Discovery
    // The update check (UpdateCheck.h): off unless turned on; when a check
    // last got through, and the newest version it announced ("" if none).
    bool checkUpdates = false;
    std::optional<std::chrono::system_clock::time_point> updateCheckedUtc;
    std::string latestVersion;
    bool operator==(const Settings&) const = default;
};

// Reads settings.json text. Never fails: missing or invalid values keep
// their defaults (and `warning` says what was ignored, if anything).
Settings parseSettings(std::string_view json, std::string& warning);
std::string settingsToJson(const Settings& settings);

// The interface scale for a textSize value (1.0 for anything unknown).
float interfaceScale(std::string_view textSize);

// Loads the file (start-up, before the window opens). A missing file gives
// the defaults.
Settings loadSettings(const std::filesystem::path& file, std::string& warning);

class SettingsStore {
public:
    explicit SettingsStore(std::filesystem::path file);
    ~SettingsStore();  // writes what is still pending
    SettingsStore(const SettingsStore&) = delete;
    SettingsStore& operator=(const SettingsStore&) = delete;
    SettingsStore(SettingsStore&&) = delete;
    SettingsStore& operator=(SettingsStore&&) = delete;

    // Any thread; returns at once. Only the latest settings are written.
    void save(const Settings& settings);
    // The last write's error, empty when it worked.
    [[nodiscard]] std::string lastError() const;
    // Blocks until everything saved so far is on disk. For tests.
    void flush();

private:
    void run();

    const std::filesystem::path m_file;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_written;
    std::optional<Settings> m_pending;
    bool m_writing = false;
    bool m_quit = false;
    std::string m_error;
    std::thread m_worker;
};

}  // namespace zarescope::app
