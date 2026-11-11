// SPDX-License-Identifier: GPL-3.0-or-later

#include "sat/SatCatalog.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <sstream>

namespace zarescope::sat {

std::optional<std::vector<Satellite>> parseCatalog(std::string_view text, std::string& error) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error& e) {
        error = std::string("satellite catalog: not valid JSON: ") + e.what();
        return std::nullopt;
    }
    try {
        if (json.value("zarescopeCatalog", 0) != 1) {
            error = "satellite catalog: unknown format version";
            return std::nullopt;
        }
        std::vector<Satellite> satellites;
        std::set<uint32_t> ids;
        for (const auto& entry : json.at("satellites")) {
            Satellite sat;
            sat.noradId = entry.at("noradId").get<uint32_t>();
            sat.name = entry.at("name").get<std::string>();
            sat.active = entry.value("active", true);
            sat.satdumpPipeline = entry.value("satdumpPipeline", "");
            sat.recordSampleRateSps = entry.value("recordSampleRateSps", 0U);
            for (const auto& link : entry.at("downlinks")) {
                Downlink downlink;
                downlink.frequencyHz = link.at("frequencyHz").get<int64_t>();
                downlink.mode = link.at("mode").get<std::string>();
                downlink.bandwidthHz = link.value("bandwidthHz", 0.0);
                if (downlink.frequencyHz <= 0) {
                    error = "satellite catalog: " + sat.name + " has an invalid downlink frequency";
                    return std::nullopt;
                }
                sat.downlinks.push_back(std::move(downlink));
            }
            if (sat.noradId == 0 || sat.name.empty() || !ids.insert(sat.noradId).second) {
                error = "satellite catalog: NORAD ids must be unique and names non-empty";
                return std::nullopt;
            }
            satellites.push_back(std::move(sat));
        }
        return satellites;
    } catch (const nlohmann::json::exception& e) {
        error = std::string("satellite catalog: ") + e.what();
        return std::nullopt;
    }
}

std::optional<std::vector<Satellite>> loadCatalog(const std::filesystem::path& path,
                                                  std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path.string();
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return parseCatalog(text.str(), error);
}

}  // namespace zarescope::sat
