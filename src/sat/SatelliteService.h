// SPDX-License-Identifier: GPL-3.0-or-later
//
// SatelliteService: keeps the TLEs fresh and the pass list current, on its
// own worker thread (network and orbit maths never run on the UI thread).
//
// TLEs come from CelesTrak's GP API, one satellite at a time by NORAD id:
//   https://celestrak.org/NORAD/elements/gp.php?CATNR=<id>&FORMAT=TLE
// following CelesTrak's usage policy: each satellite is
// fetched at most once every 12 hours, with an identifiable User-Agent, and
// the answer is cached on disk (tle_cache/<id>.json). Offline, the cache is
// used, and the UI shows how old it is.
//
// Passes are predicted for the next 48 hours from the station whenever a
// TLE, the station or the minimum elevation changes, and every 10 minutes.

#pragma once

#include "sat/Orbit.h"
#include "sat/SatCatalog.h"

#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace zarescope::sat {

struct TleInfo {
    Tle tle;
    Clock::time_point fetched;  // when it was downloaded
};

struct PlannedPass {
    uint32_t noradId = 0;
    std::string satellite;
    Pass pass;
};

struct SatelliteSnapshot {
    std::map<uint32_t, TleInfo> tles;  // by NORAD id
    std::vector<PlannedPass> passes;   // all satellites, by AOS
    bool fetching = false;
    std::string lastError;  // the last fetch that failed, if any
    std::optional<Station> station;
    double minElevationDeg = 20.0;
};

class SatelliteService {
public:
    static constexpr auto kRefreshInterval = std::chrono::hours(12);
    static constexpr auto kWindow = std::chrono::hours(48);

    // `cacheDir` is created on first write. `userAgent` identifies the app
    // to CelesTrak, e.g. "Zarescope/0.5.0 (+https://zarescope.eu)".
    SatelliteService(std::vector<Satellite> catalog, std::filesystem::path cacheDir,
                     std::string userAgent);
    ~SatelliteService();
    SatelliteService(const SatelliteService&) = delete;
    SatelliteService& operator=(const SatelliteService&) = delete;
    SatelliteService(SatelliteService&&) = delete;
    SatelliteService& operator=(SatelliteService&&) = delete;

    // UI thread; return at once.
    void setStation(std::optional<Station> station);
    void setMinElevation(double degrees);
    // Fetches every TLE older than 12 h (never more often).
    void refresh();

    [[nodiscard]] SatelliteSnapshot snapshot() const;
    [[nodiscard]] const std::vector<Satellite>& catalog() const { return m_catalog; }

private:
    void run();
    void loadCache();
    void fetchStale();
    void predict();

    const std::vector<Satellite> m_catalog;
    const std::filesystem::path m_cacheDir;
    const std::string m_userAgent;

    mutable std::mutex m_mutex;  // guards everything below
    std::condition_variable m_wake;
    SatelliteSnapshot m_snapshot;
    bool m_refreshRequested = true;  // fetch on start
    bool m_predictRequested = true;
    bool m_quit = false;
    std::thread m_worker;
};

// Cache files: the TLE text plus the time it was fetched (docs/FORMATS.md).
std::optional<TleInfo> readCachedTle(const std::filesystem::path& file, std::string& error);
bool writeCachedTle(const std::filesystem::path& file, const TleInfo& info, std::string& error);

}  // namespace zarescope::sat
