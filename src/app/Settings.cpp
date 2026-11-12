// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/Settings.h"

#include "app/UpdateCheck.h"      // version numbers
#include "device/IqMetadata.h"    // UTC timestamps
#include "device/RtlSdrSource.h"  // the sample rates a stick accepts

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace zarescope::app {

namespace {

constexpr int kFormatVersion = 1;

// Reads one value of type T, or leaves `out` alone (and notes it) when the
// key is there with the wrong type.
template <typename T>
void read(const nlohmann::json& json, const char* key, T& out, std::string& warning) {
    const auto it = json.find(key);
    if (it == json.end() || it->is_null()) {
        return;
    }
    try {
        out = it->get<T>();
    } catch (const nlohmann::json::exception&) {
        warning += std::string(warning.empty() ? "" : "; ") + key + " ignored";
    }
}

}  // namespace

Settings parseSettings(std::string_view text, std::string& warning) {
    Settings settings;
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error&) {
        warning = "settings.json is not valid JSON; using the defaults";
        return settings;
    }
    if (!json.is_object()) {
        warning = "settings.json is not an object; using the defaults";
        return settings;
    }
    read(json, "theme", settings.theme, warning);
    if (settings.theme != "dark" && settings.theme != "light") {
        settings.theme = "dark";
    }
    read(json, "language", settings.language, warning);
    if (settings.language != "en" && settings.language != "ro") {
        settings.language = "en";
    }
    read(json, "textSize", settings.textSize, warning);
    if (settings.textSize != "standard" && settings.textSize != "large" &&
        settings.textSize != "larger") {
        settings.textSize = "standard";
    }
    read(json, "units", settings.units, warning);
    if (settings.units != "metric" && settings.units != "aviation" &&
        settings.units != "imperial") {
        settings.units = "metric";
    }
    if (const auto it = json.find("station"); it != json.end() && it->is_object()) {
        StationSetting station;
        read(*it, "latDeg", station.latDeg, warning);
        read(*it, "lonDeg", station.lonDeg, warning);
        read(*it, "altitudeM", station.altitudeM, warning);
        const bool valid = std::isfinite(station.latDeg) && std::isfinite(station.lonDeg) &&
                           std::abs(station.latDeg) <= 90.0 && std::abs(station.lonDeg) <= 180.0 &&
                           std::isfinite(station.altitudeM);
        if (valid) {
            station.altitudeM = std::clamp(station.altitudeM, -500.0, 9000.0);
            settings.station = station;
        } else {
            warning += std::string(warning.empty() ? "" : "; ") + "station ignored";
        }
    }
    read(json, "minElevationDeg", settings.minElevationDeg, warning);
    if (!std::isfinite(settings.minElevationDeg)) {
        settings.minElevationDeg = 20.0;
    }
    settings.minElevationDeg = std::clamp(settings.minElevationDeg, 0.0, 60.0);
    read(json, "autoRecord", settings.autoRecord, warning);
    read(json, "keepBaseband", settings.keepBaseband, warning);
    read(json, "defaultSampleRateSps", settings.defaultSampleRateSps, warning);
    if (std::find(std::begin(device::kSampleRates), std::end(device::kSampleRates),
                  settings.defaultSampleRateSps) == std::end(device::kSampleRates)) {
        settings.defaultSampleRateSps = 2'400'000;
    }
    read(json, "ppm", settings.ppm, warning);
    settings.ppm = std::clamp(settings.ppm, -200, 200);
    read(json, "dataFolder", settings.dataFolder, warning);
    read(json, "discoverySeen", settings.discoverySeen, warning);
    read(json, "checkUpdates", settings.checkUpdates, warning);
    std::string checked;
    read(json, "updateCheckedUtc", checked, warning);
    if (!checked.empty()) {
        settings.updateCheckedUtc = device::parseUtcTimestamp(checked);
    }
    read(json, "latestVersion", settings.latestVersion, warning);
    if (const auto version = parseVersion(settings.latestVersion)) {
        settings.latestVersion = formatVersion(*version);
    } else {
        settings.latestVersion.clear();
    }
    return settings;
}

std::string settingsToJson(const Settings& settings) {
    nlohmann::ordered_json json;
    json["zarescopeSettings"] = kFormatVersion;
    json["theme"] = settings.theme;
    json["language"] = settings.language;
    json["textSize"] = settings.textSize;
    json["units"] = settings.units;
    if (settings.station) {
        json["station"] = {{"latDeg", settings.station->latDeg},
                           {"lonDeg", settings.station->lonDeg},
                           {"altitudeM", settings.station->altitudeM}};
    } else {
        json["station"] = nullptr;
    }
    json["minElevationDeg"] = settings.minElevationDeg;
    json["autoRecord"] = settings.autoRecord;
    json["keepBaseband"] = settings.keepBaseband;
    json["defaultSampleRateSps"] = settings.defaultSampleRateSps;
    json["ppm"] = settings.ppm;
    json["dataFolder"] = settings.dataFolder;
    json["discoverySeen"] = settings.discoverySeen;
    json["checkUpdates"] = settings.checkUpdates;
    json["updateCheckedUtc"] = settings.updateCheckedUtc
                                   ? device::formatUtcTimestamp(*settings.updateCheckedUtc)
                                   : std::string{};
    json["latestVersion"] = settings.latestVersion;
    return json.dump(2) + "\n";
}

float interfaceScale(std::string_view textSize) {
    // Large and Larger stay within what a 13-inch MacBook Air's default
    // resolution (1470 points wide) can show at the minimum window width.
    if (textSize == "large") {
        return 1.1F;
    }
    if (textSize == "larger") {
        return 1.2F;
    }
    return 1.0F;
}

Settings loadSettings(const std::filesystem::path& file, std::string& warning) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return {};  // first run
    }
    std::ostringstream text;
    text << in.rdbuf();
    return parseSettings(text.str(), warning);
}

SettingsStore::SettingsStore(std::filesystem::path file) : m_file(std::move(file)) {
    m_worker = std::thread([this] { run(); });
}

SettingsStore::~SettingsStore() {
    {
        const std::scoped_lock lock(m_mutex);
        m_quit = true;
    }
    m_wake.notify_all();
    m_worker.join();
}

void SettingsStore::save(const Settings& settings) {
    {
        const std::scoped_lock lock(m_mutex);
        m_pending = settings;
    }
    m_wake.notify_all();
}

std::string SettingsStore::lastError() const {
    const std::scoped_lock lock(m_mutex);
    return m_error;
}

void SettingsStore::flush() {
    std::unique_lock lock(m_mutex);
    m_written.wait(lock, [this] { return !m_pending && !m_writing; });
}

void SettingsStore::run() {
    std::unique_lock lock(m_mutex);
    while (true) {
        m_wake.wait(lock, [this] { return m_quit || m_pending.has_value(); });
        if (m_pending) {
            const Settings settings = *m_pending;
            m_pending.reset();
            m_writing = true;
            lock.unlock();

            std::string error;
            std::error_code ec;
            std::filesystem::create_directories(m_file.parent_path(), ec);
            const std::filesystem::path temporary = m_file.string() + ".tmp";
            {
                std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                out << settingsToJson(settings);
                if (!out) {
                    error = "cannot write " + temporary.string();
                }
            }
            if (error.empty()) {
                std::filesystem::rename(temporary, m_file, ec);
                if (ec) {
                    error = "cannot replace " + m_file.string() + ": " + ec.message();
                }
            }

            lock.lock();
            m_error = error;
            m_writing = false;
            m_written.notify_all();
        } else if (m_quit) {
            break;
        }
    }
}

}  // namespace zarescope::app
