// SPDX-License-Identifier: GPL-3.0-or-later
//
// SatCatalog: the satellites Zarescope knows how to receive, read from
// resources/sat_catalog.json (format: docs/FORMATS.md).
//
// Frequencies come from this file and never from code (a rule since M4): the
// Meteor satellites have changed frequency without notice before, and a
// catalog edit is all that's needed when they do.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zarescope::sat {

struct Downlink {
    int64_t frequencyHz = 0;
    std::string mode;  // e.g. "lrpt"
    double bandwidthHz = 0.0;
};

struct Satellite {
    uint32_t noradId = 0;
    std::string name;
    bool active = true;
    std::vector<Downlink> downlinks;
    std::string satdumpPipeline;  // checked against `satdump --help` in M5
    uint32_t recordSampleRateSps = 0;
};

std::optional<std::vector<Satellite>> parseCatalog(std::string_view json, std::string& error);
std::optional<std::vector<Satellite>> loadCatalog(const std::filesystem::path& path,
                                                  std::string& error);

}  // namespace zarescope::sat
