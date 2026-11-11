// SPDX-License-Identifier: GPL-3.0-or-later
//
// IqMetadata: the JSON sidecar next to every .cu8 recording.
//
// A raw .cu8 file is only bytes; the sidecar says what they are (centre
// frequency, sample rate, start time, gain). Format: docs/FORMATS.md.
//   recordings/2026-11-11T210705Z_100000000Hz.cu8
//   recordings/2026-11-11T210705Z_100000000Hz.json

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace zarescope::device {

struct IqMetadata {
    static constexpr int kFormatVersion = 1;

    int64_t centerFrequencyHz = 0;
    uint32_t sampleRateSps = 0;
    std::chrono::system_clock::time_point startUtc;  // first sample, UTC
    std::optional<int> gainTenthsDb;                 // nullopt = automatic gain
    int ppm = 0;
    std::string device;                   // e.g. "RTL-SDR Blog V4 (R828D)"
    std::string software;                 // e.g. "Zarescope 0.2.0"
    std::optional<uint64_t> sampleCount;  // written when the recording ends
};

// Serialises to the sidecar's JSON text (pretty-printed, stable key order).
std::string toJson(const IqMetadata& metadata);

// Parses sidecar JSON. Returns std::nullopt and fills `error` on a missing
// or invalid field.
std::optional<IqMetadata> metadataFromJson(std::string_view json, std::string& error);

// The sidecar path for a recording: same folder and name, ".json".
std::filesystem::path sidecarPath(const std::filesystem::path& recording);

// File helpers. Not for the UI thread (disk I/O).
bool writeSidecar(const std::filesystem::path& recording, const IqMetadata& metadata,
                  std::string& error);
std::optional<IqMetadata> readSidecar(const std::filesystem::path& recording, std::string& error);

// "2026-11-11T21:07:05.750Z" ↔ time point (millisecond precision).
std::string formatUtcTimestamp(std::chrono::system_clock::time_point when);
std::optional<std::chrono::system_clock::time_point> parseUtcTimestamp(std::string_view text);

// A file name for a new recording: "2026-11-11T210705Z_100000000Hz.cu8".
std::string recordingFileName(std::chrono::system_clock::time_point startUtc,
                              int64_t centerFrequencyHz);

}  // namespace zarescope::device
