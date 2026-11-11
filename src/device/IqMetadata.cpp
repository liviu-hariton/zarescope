// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/IqMetadata.h"

#include <nlohmann/json.hpp>

#include <charconv>
#include <format>
#include <fstream>
#include <sstream>

namespace zarescope::device {

namespace {

using nlohmann::ordered_json;
using namespace std::chrono;

// Parses exactly `width` digits at `text[pos]`.
std::optional<int> digits(std::string_view text, size_t pos, size_t width) {
    if (pos + width > text.size()) {
        return std::nullopt;
    }
    int value = 0;
    auto [end, error] = std::from_chars(text.data() + pos, text.data() + pos + width, value);
    if (error != std::errc{} || end != text.data() + pos + width) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

std::string formatUtcTimestamp(system_clock::time_point when) {
    return std::format("{:%Y-%m-%dT%H:%M:%S}Z", floor<milliseconds>(when));
}

std::optional<system_clock::time_point> parseUtcTimestamp(std::string_view text) {
    // YYYY-MM-DDTHH:MM:SS[.fff]Z
    if (text.size() < 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text.back() != 'Z') {
        return std::nullopt;
    }
    const auto y = digits(text, 0, 4);
    const auto mo = digits(text, 5, 2);
    const auto d = digits(text, 8, 2);
    const auto h = digits(text, 11, 2);
    const auto mi = digits(text, 14, 2);
    const auto s = digits(text, 17, 2);
    if (!y || !mo || !d || !h || !mi || !s || *h > 23 || *mi > 59 || *s > 60) {
        return std::nullopt;
    }
    int millis = 0;
    if (text.size() > 20) {
        if (text[19] != '.') {
            return std::nullopt;
        }
        const std::string_view fraction = text.substr(20, text.size() - 21);
        if (fraction.empty() || fraction.size() > 9) {
            return std::nullopt;
        }
        const auto value = digits(fraction, 0, std::min<size_t>(3, fraction.size()));
        if (!value) {
            return std::nullopt;
        }
        millis = *value;
        for (size_t i = fraction.size(); i < 3; ++i) {
            millis *= 10;
        }
    } else if (text.size() != 20) {
        return std::nullopt;
    }
    const year_month_day date{year{*y}, month{static_cast<unsigned>(*mo)},
                              day{static_cast<unsigned>(*d)}};
    if (!date.ok()) {
        return std::nullopt;
    }
    return sys_days{date} + hours{*h} + minutes{*mi} + seconds{*s} + milliseconds{millis};
}

std::string recordingFileName(system_clock::time_point startUtc, int64_t centerFrequencyHz) {
    return std::format("{:%Y-%m-%dT%H%M%S}Z_{}Hz.cu8", floor<seconds>(startUtc), centerFrequencyHz);
}

std::string toJson(const IqMetadata& metadata) {
    ordered_json json;
    json["zarescopeIq"] = IqMetadata::kFormatVersion;
    json["format"] = "cu8";
    json["centerFrequencyHz"] = metadata.centerFrequencyHz;
    json["sampleRateSps"] = metadata.sampleRateSps;
    json["startUtc"] = formatUtcTimestamp(metadata.startUtc);
    if (metadata.gainTenthsDb) {
        json["gainTenthsDb"] = *metadata.gainTenthsDb;
    } else {
        json["gainTenthsDb"] = nullptr;  // automatic gain
    }
    json["ppm"] = metadata.ppm;
    json["device"] = metadata.device;
    json["software"] = metadata.software;
    if (metadata.sampleCount) {
        json["sampleCount"] = *metadata.sampleCount;
    }
    return json.dump(2) + "\n";
}

std::optional<IqMetadata> metadataFromJson(std::string_view text, std::string& error) {
    ordered_json json;
    try {
        json = ordered_json::parse(text);
    } catch (const ordered_json::parse_error& e) {
        error = std::string("not valid JSON: ") + e.what();
        return std::nullopt;
    }
    try {
        if (json.value("format", "") != "cu8") {
            error = "only \"format\": \"cu8\" recordings are supported";
            return std::nullopt;
        }
        IqMetadata metadata;
        metadata.centerFrequencyHz = json.at("centerFrequencyHz").get<int64_t>();
        metadata.sampleRateSps = json.at("sampleRateSps").get<uint32_t>();
        if (metadata.sampleRateSps == 0) {
            error = "sampleRateSps must be positive";
            return std::nullopt;
        }
        auto start = parseUtcTimestamp(json.at("startUtc").get<std::string>());
        if (!start) {
            error = "startUtc must look like 2026-11-11T21:07:05.750Z";
            return std::nullopt;
        }
        metadata.startUtc = *start;
        if (json.contains("gainTenthsDb") && !json["gainTenthsDb"].is_null()) {
            metadata.gainTenthsDb = json["gainTenthsDb"].get<int>();
        }
        metadata.ppm = json.value("ppm", 0);
        metadata.device = json.value("device", "");
        metadata.software = json.value("software", "");
        if (json.contains("sampleCount")) {
            metadata.sampleCount = json["sampleCount"].get<uint64_t>();
        }
        return metadata;
    } catch (const ordered_json::exception& e) {
        error = std::string("missing or invalid field: ") + e.what();
        return std::nullopt;
    }
}

std::filesystem::path sidecarPath(const std::filesystem::path& recording) {
    std::filesystem::path path = recording;
    path.replace_extension(".json");
    return path;
}

bool writeSidecar(const std::filesystem::path& recording, const IqMetadata& metadata,
                  std::string& error) {
    const auto path = sidecarPath(recording);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << toJson(metadata);
    if (!out) {
        error = "cannot write " + path.string();
        return false;
    }
    return true;
}

std::optional<IqMetadata> readSidecar(const std::filesystem::path& recording, std::string& error) {
    const auto path = sidecarPath(recording);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "missing sidecar file " + path.filename().string() +
                " (it holds the frequency and sample rate)";
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    auto metadata = metadataFromJson(text.str(), error);
    if (!metadata) {
        error = path.filename().string() + ": " + error;
    }
    return metadata;
}

}  // namespace zarescope::device
