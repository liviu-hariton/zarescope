// SPDX-License-Identifier: GPL-3.0-or-later
//
// Presets: ready-made settings for common things to listen to, read from
// resources/presets.json (format: docs/FORMATS.md).
//
// A preset holds data only: an id, a frequency, a mode and a channel width.
// Its name and description are UI text and come from the string table by
// id, so they exist in every language.

#pragma once

#include "dsp/AudioDemodulator.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zarescope::device {

struct Preset {
    std::string id;  // e.g. "fm-broadcast"
    int64_t frequencyHz = 0;
    dsp::DemodMode mode = dsp::DemodMode::Off;
    double bandwidthHz = 0.0;    // 0 = the mode's default
    uint32_t sampleRateSps = 0;  // 0 = keep the current rate (ADS-B needs 2 MS/s)
};

// Parses the presets file's text. Returns std::nullopt and fills `error`
// on a malformed file or entry.
std::optional<std::vector<Preset>> parsePresets(std::string_view json, std::string& error);
std::optional<std::vector<Preset>> loadPresets(const std::filesystem::path& path,
                                               std::string& error);

// "wfm" / "nfm" / "am" / "off" ↔ mode.
std::optional<dsp::DemodMode> parseMode(std::string_view text);

}  // namespace zarescope::device
