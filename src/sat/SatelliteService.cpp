// SPDX-License-Identifier: GPL-3.0-or-later

#include "sat/SatelliteService.h"

#include "device/IqMetadata.h"  // UTC timestamp format

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace zarescope::sat {

namespace {

constexpr auto kPredictEvery = std::chrono::minutes(10);
constexpr long kTimeoutSeconds = 20;
constexpr size_t kMaxResponseBytes = 16 * 1024;  // a TLE is under 200 bytes

size_t collect(char* data, size_t size, size_t count, void* user) {
    auto* body = static_cast<std::string*>(user);
    const size_t bytes = size * count;
    if (body->size() + bytes > kMaxResponseBytes) {
        return 0;  // refuse oversized answers
    }
    body->append(data, bytes);
    return bytes;
}

// One GET with libcurl. Returns the body, or std::nullopt with `error`.
std::optional<std::string> httpGet(const std::string& url, const std::string& userAgent,
                                   std::string& error) {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        error = "network unavailable";
        return std::nullopt;
    }
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK) {
        error = curl_easy_strerror(result);
        return std::nullopt;
    }
    if (status != 200) {
        error = "CelesTrak answered HTTP " + std::to_string(status);
        return std::nullopt;
    }
    return body;
}

}  // namespace

std::optional<TleInfo> readCachedTle(const std::filesystem::path& file, std::string& error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        error = "no cached TLE";
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    try {
        const auto json = nlohmann::json::parse(text.str());
        const auto fetched = device::parseUtcTimestamp(json.at("fetchedUtc").get<std::string>());
        auto tle = parseTle(json.at("tle").get<std::string>(), error);
        if (!fetched || !tle) {
            error = "cached TLE is damaged";
            return std::nullopt;
        }
        return TleInfo{*tle, *fetched};
    } catch (const nlohmann::json::exception& e) {
        error = std::string("cached TLE is damaged: ") + e.what();
        return std::nullopt;
    }
}

bool writeCachedTle(const std::filesystem::path& file, const TleInfo& info, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    nlohmann::ordered_json json;
    json["fetchedUtc"] = device::formatUtcTimestamp(info.fetched);
    json["tle"] = info.tle.name + "\n" + info.tle.line1 + "\n" + info.tle.line2 + "\n";
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << json.dump(2) << "\n";
    if (!out) {
        error = "cannot write " + file.string();
        return false;
    }
    return true;
}

SatelliteService::SatelliteService(std::vector<Satellite> catalog, std::filesystem::path cacheDir,
                                   std::string userAgent)
    : m_catalog(std::move(catalog)),
      m_cacheDir(std::move(cacheDir)),
      m_userAgent(std::move(userAgent)) {
    m_worker = std::thread([this] { run(); });
}

SatelliteService::~SatelliteService() {
    {
        const std::scoped_lock lock(m_mutex);
        m_quit = true;
    }
    m_wake.notify_all();
    m_worker.join();
}

void SatelliteService::setStation(std::optional<Station> station) {
    {
        const std::scoped_lock lock(m_mutex);
        const bool same = station.has_value() == m_snapshot.station.has_value() &&
                          (!station || (station->latDeg == m_snapshot.station->latDeg &&
                                        station->lonDeg == m_snapshot.station->lonDeg &&
                                        station->altitudeM == m_snapshot.station->altitudeM));
        if (same) {
            return;
        }
        m_snapshot.station = station;
        m_predictRequested = true;
    }
    m_wake.notify_all();
}

void SatelliteService::setMinElevation(double degrees) {
    {
        const std::scoped_lock lock(m_mutex);
        if (degrees == m_snapshot.minElevationDeg) {
            return;
        }
        m_snapshot.minElevationDeg = degrees;
        m_predictRequested = true;
    }
    m_wake.notify_all();
}

void SatelliteService::refresh() {
    {
        const std::scoped_lock lock(m_mutex);
        m_refreshRequested = true;
    }
    m_wake.notify_all();
}

SatelliteSnapshot SatelliteService::snapshot() const {
    const std::scoped_lock lock(m_mutex);
    return m_snapshot;
}

void SatelliteService::run() {
    loadCache();
    auto lastPrediction = Clock::now();
    std::unique_lock lock(m_mutex);
    while (!m_quit) {
        if (m_refreshRequested) {
            m_refreshRequested = false;
            m_snapshot.fetching = true;
            lock.unlock();
            fetchStale();
            lock.lock();
            m_snapshot.fetching = false;
        }
        if (m_predictRequested || Clock::now() - lastPrediction > kPredictEvery) {
            m_predictRequested = false;
            lock.unlock();
            predict();
            lock.lock();
            lastPrediction = Clock::now();
        }
        m_wake.wait_for(lock, std::chrono::minutes(1),
                        [this] { return m_quit || m_refreshRequested || m_predictRequested; });
    }
}

void SatelliteService::loadCache() {
    for (const Satellite& sat : m_catalog) {
        std::string error;
        if (auto info =
                readCachedTle(m_cacheDir / (std::to_string(sat.noradId) + ".json"), error)) {
            const std::scoped_lock lock(m_mutex);
            m_snapshot.tles[sat.noradId] = *info;
            m_predictRequested = true;
        }
    }
}

void SatelliteService::fetchStale() {
    for (const Satellite& sat : m_catalog) {
        if (!sat.active) {
            continue;
        }
        {
            const std::scoped_lock lock(m_mutex);
            const auto it = m_snapshot.tles.find(sat.noradId);
            if (it != m_snapshot.tles.end() &&
                Clock::now() - it->second.fetched < kRefreshInterval) {
                continue;  // fresh enough: CelesTrak asks not to fetch more often
            }
        }
        const std::string url =
            "https://celestrak.org/NORAD/elements/gp.php?CATNR=" + std::to_string(sat.noradId) +
            "&FORMAT=TLE";
        std::string error;
        auto body = httpGet(url, m_userAgent, error);
        std::optional<Tle> tle;
        if (body) {
            tle = parseTle(*body, error);
            if (tle && tle->noradId != sat.noradId) {
                error = "CelesTrak sent a TLE for another satellite";
                tle.reset();
            }
        }
        const std::scoped_lock lock(m_mutex);
        if (!tle) {
            m_snapshot.lastError = sat.name + ": " + error;
            continue;
        }
        const TleInfo info{*tle, Clock::now()};
        m_snapshot.tles[sat.noradId] = info;
        m_snapshot.lastError.clear();
        m_predictRequested = true;
        std::string writeError;
        writeCachedTle(m_cacheDir / (std::to_string(sat.noradId) + ".json"), info, writeError);
    }
}

void SatelliteService::predict() {
    SatelliteSnapshot input;
    {
        const std::scoped_lock lock(m_mutex);
        input = m_snapshot;
    }
    std::vector<PlannedPass> passes;
    if (input.station) {
        const auto now = Clock::now();
        for (const Satellite& sat : m_catalog) {
            const auto it = input.tles.find(sat.noradId);
            if (!sat.active || it == input.tles.end()) {
                continue;
            }
            try {
                const Propagator propagator(it->second.tle);
                for (const Pass& pass : predictPasses(propagator, *input.station, now, kWindow,
                                                      input.minElevationDeg)) {
                    passes.push_back({sat.noradId, sat.name, pass});
                }
            } catch (const std::exception&) {
                // A decayed orbit or a TLE libsgp4 rejects: no passes for it.
            }
        }
        std::sort(passes.begin(), passes.end(), [](const PlannedPass& a, const PlannedPass& b) {
            return a.pass.aos < b.pass.aos;
        });
    }
    const std::scoped_lock lock(m_mutex);
    m_snapshot.passes = std::move(passes);
}

}  // namespace zarescope::sat
