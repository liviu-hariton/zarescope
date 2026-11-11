// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/Presets.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <sstream>

namespace zarescope::device {

std::optional<dsp::DemodMode> parseMode(std::string_view text) {
    if (text == "wfm") {
        return dsp::DemodMode::Wfm;
    }
    if (text == "nfm") {
        return dsp::DemodMode::Nfm;
    }
    if (text == "am") {
        return dsp::DemodMode::Am;
    }
    if (text == "off") {
        return dsp::DemodMode::Off;
    }
    return std::nullopt;
}

std::optional<std::vector<Preset>> parsePresets(std::string_view text, std::string& error) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error& e) {
        error = std::string("presets: not valid JSON: ") + e.what();
        return std::nullopt;
    }
    try {
        if (json.value("zarescopePresets", 0) != 1) {
            error = "presets: unknown format version";
            return std::nullopt;
        }
        std::vector<Preset> presets;
        std::set<std::string> ids;
        for (const auto& entry : json.at("presets")) {
            Preset preset;
            preset.id = entry.at("id").get<std::string>();
            preset.frequencyHz = entry.at("frequencyHz").get<int64_t>();
            const auto mode = parseMode(entry.at("mode").get<std::string>());
            if (!mode) {
                error = "presets: \"" + preset.id + "\" has an unknown mode";
                return std::nullopt;
            }
            preset.mode = *mode;
            preset.bandwidthHz = entry.value("bandwidthHz", 0.0);
            preset.sampleRateSps = entry.value("sampleRateSps", 0U);
            if (preset.id.empty() || !ids.insert(preset.id).second) {
                error = "presets: ids must be unique and non-empty (\"" + preset.id + "\")";
                return std::nullopt;
            }
            if (preset.frequencyHz <= 0 || preset.bandwidthHz < 0.0) {
                error = "presets: \"" + preset.id + "\" has an invalid frequency or bandwidth";
                return std::nullopt;
            }
            presets.push_back(std::move(preset));
        }
        return presets;
    } catch (const nlohmann::json::exception& e) {
        error = std::string("presets: ") + e.what();
        return std::nullopt;
    }
}

std::optional<std::vector<Preset>> loadPresets(const std::filesystem::path& path,
                                               std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path.string();
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return parsePresets(text.str(), error);
}

}  // namespace zarescope::device
