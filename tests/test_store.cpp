// SPDX-License-Identifier: GPL-3.0-or-later
//
// M6: settings.json and the journal (SQLite): round trips, bad input,
// migrations, filters, exports, the worker-thread services.

#include "app/Settings.h"
#include "store/Journal.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <fstream>
#include <random>

using namespace zarescope;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        std::random_device random;
        path = fs::temp_directory_path() /
               ("zarescope-test-" + std::to_string(random()) + std::to_string(random()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;
};

const store::Clock::time_point kT0 = store::Clock::from_time_t(1'800'000'000);

store::JournalEvent event(store::EventKind kind, std::string title, store::Clock::duration at,
                          std::string detail = {}, std::string key = {}) {
    store::JournalEvent e;
    e.kind = kind;
    e.title = std::move(title);
    e.timeUtc = kT0 + at;
    e.detail = std::move(detail);
    e.key = std::move(key);
    return e;
}

}  // namespace

// --- Settings ----------------------------------------------------------------------

TEST_CASE("settings: round trip through JSON") {
    app::Settings settings;
    settings.theme = "light";
    settings.language = "ro";
    settings.textSize = "larger";
    settings.station = app::StationSetting{44.4268, 26.1025, 80.0};
    settings.minElevationDeg = 25.0;
    settings.autoRecord = true;
    settings.keepBaseband = true;
    settings.defaultSampleRateSps = 2'048'000;
    settings.ppm = -3;
    settings.dataFolder = "/Volumes/Data/Zarescope";
    settings.discoverySeen = true;
    std::string warning;
    CHECK(app::parseSettings(app::settingsToJson(settings), warning) == settings);
    CHECK(warning.empty());
}

TEST_CASE("settings: bad or missing values fall back to defaults, never fail") {
    std::string warning;
    CHECK(app::parseSettings("not json", warning) == app::Settings{});
    CHECK_FALSE(warning.empty());

    warning.clear();
    const auto s = app::parseSettings(R"({
        "theme": "purple", "language": 7, "station": {"latDeg": 123, "lonDeg": 0},
        "minElevationDeg": 95, "defaultSampleRateSps": 1234, "ppm": 9999,
        "autoRecord": "yes", "somethingNew": true})", warning);
    CHECK(s.theme == "dark");
    CHECK(s.textSize == "standard");
    CHECK(s.language == "en");
    CHECK_FALSE(s.station);              // latitude out of range
    CHECK(s.minElevationDeg == 60.0);    // clamped
    CHECK(s.defaultSampleRateSps == 2'400'000);  // not a rate the stick takes
    CHECK(s.ppm == 200);
    CHECK_FALSE(s.autoRecord);           // wrong type: ignored
    CHECK(warning.find("autoRecord") != std::string::npos);
}

TEST_CASE("settings: text sizes scale the interface, within a laptop's width") {
    CHECK(app::interfaceScale("standard") == 1.0F);
    CHECK(app::interfaceScale("large") > 1.0F);
    CHECK(app::interfaceScale("larger") > app::interfaceScale("large"));
    CHECK(app::interfaceScale("huge") == 1.0F);
    // A 13-inch MacBook Air shows 1470 points across by default.
    CHECK(1200.0F * app::interfaceScale("larger") <= 1470.0F);
}

TEST_CASE("settings: the store writes the latest copy, atomically") {
    const TempDir dir;
    const fs::path file = dir.path / "sub" / "settings.json";
    std::string warning;
    CHECK(app::loadSettings(file, warning) == app::Settings{});  // first run
    app::Settings settings;
    {
        app::SettingsStore store(file);
        for (int ppm = 1; ppm <= 20; ++ppm) {
            settings.ppm = ppm;
            store.save(settings);
        }
        store.flush();
        CHECK(store.lastError().empty());
        CHECK(app::loadSettings(file, warning).ppm == 20);
        settings.autoRecord = true;
        store.save(settings);
    }  // the destructor writes what is pending
    CHECK(app::loadSettings(file, warning) == settings);
    CHECK_FALSE(fs::exists(dir.path / "sub" / "settings.json.tmp"));
}

// --- Journal -----------------------------------------------------------------------

TEST_CASE("journal: a new file is migrated to the current schema") {
    const TempDir dir;
    std::string error;
    auto journal = store::Journal::open(dir.path / "journal.sqlite", error);
    REQUIRE(journal);
    CHECK(error.empty());
    CHECK(journal->schemaVersion() == store::Journal::kSchemaVersion);
    journal.reset();
    // Opening again doesn't migrate twice.
    journal = store::Journal::open(dir.path / "journal.sqlite", error);
    REQUIRE(journal);
    CHECK(journal->schemaVersion() == store::Journal::kSchemaVersion);
}

TEST_CASE("journal: a file from a newer Zarescope is refused and left alone") {
    const TempDir dir;
    const fs::path file = dir.path / "journal.sqlite";
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(file.string().c_str(), &db) == SQLITE_OK);
    sqlite3_exec(db, "PRAGMA user_version = 99", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    std::string error;
    CHECK_FALSE(store::Journal::open(file, error));
    CHECK(error.find("newer") != std::string::npos);
}

TEST_CASE("journal: events are stored, filtered and keyed rows updated in place") {
    const TempDir dir;
    std::string error;
    auto journal = store::Journal::open(dir.path / "journal.sqlite", error);
    REQUIRE(journal);
    REQUIRE(journal->add(event(store::EventKind::Pass, "Meteor-M N2-4", 0min, "no image"), error));
    REQUIRE(journal->add(event(store::EventKind::Images, "Meteor-M N2-4", 1min, "4 images"), error));
    REQUIRE(journal->add(event(store::EventKind::Recording, "100% FM", 2min, "100_000 Hz"), error));
    // The same aircraft twice on one day: one row, the newer detail, the first time.
    REQUIRE(journal->add(event(store::EventKind::Aircraft, "4CA1FA", 3min, "4CA1FA",
                               "aircraft:2027-01-15:4CA1FA"),
                         error));
    REQUIRE(journal->add(event(store::EventKind::Aircraft, "RYR4TG", 9min, "RYR4TG · 4CA1FA",
                               "aircraft:2027-01-15:4CA1FA"),
                         error));

    auto all = journal->query({}, error);
    REQUIRE(all.size() == 4);
    CHECK(all.front().kind == store::EventKind::Aircraft);  // newest first
    CHECK(all.front().title == "RYR4TG");
    CHECK(all.front().timeUtc == kT0 + 3min);
    CHECK(all.back().kind == store::EventKind::Pass);

    store::JournalFilter filter;
    filter.kind = store::EventKind::Images;
    CHECK(journal->query(filter, error).size() == 1);

    filter = {};
    filter.text = "n2-4";  // case-insensitive, title or detail
    CHECK(journal->query(filter, error).size() == 2);
    filter.text = "100%";  // % is matched literally, not as a wildcard
    CHECK(journal->query(filter, error).size() == 1);
    filter.text = "_";
    CHECK(journal->query(filter, error).size() == 1);  // only "100_000 Hz"

    filter = {};
    filter.from = kT0 + 1min;
    filter.to = kT0 + 3min;
    CHECK(journal->query(filter, error).size() == 2);
    filter = {};
    filter.limit = 2;
    CHECK(journal->query(filter, error).size() == 2);
    CHECK(error.empty());
}

TEST_CASE("journal: CSV and JSON exports") {
    auto e = event(store::EventKind::Pass, "Meteor \"N2-4\"", 0min, "a, b");
    e.data = R"({"images":4})";
    const std::string csv = store::toCsv({e});
    CHECK(csv.starts_with("time_utc,kind,title,detail,path,data\r\n"));
    CHECK(csv.find(R"("Meteor ""N2-4""")") != std::string::npos);  // quotes doubled
    CHECK(csv.find(R"("a, b")") != std::string::npos);

    const auto json = nlohmann::json::parse(store::toJson({e}));
    REQUIRE(json.size() == 1);
    CHECK(json[0]["kind"] == "pass");
    CHECK(json[0]["data"]["images"] == 4);  // embedded as an object
    CHECK(json[0]["timeUtc"] == "2027-01-15T08:00:00.000Z");
}

TEST_CASE("journal service: adds, queries and exports on its own thread") {
    const TempDir dir;
    store::JournalService service(dir.path / "journal.sqlite");
    service.add(event(store::EventKind::Pass, "Meteor-M N2-3", 0min));
    service.add(event(store::EventKind::Recording, "rec", 1min));
    service.flush();
    auto snapshot = service.snapshot();
    CHECK(snapshot.open);
    CHECK(snapshot.events.size() == 2);

    store::JournalFilter filter;
    filter.kind = store::EventKind::Recording;
    service.setFilter(filter);
    service.exportTo(dir.path / "out.csv", false);
    service.flush();
    snapshot = service.snapshot();
    CHECK(snapshot.events.size() == 1);
    CHECK(snapshot.exportGeneration == 1);
    std::ifstream in(dir.path / "out.csv");
    std::string header;
    std::string row;
    std::getline(in, header);
    std::getline(in, row);
    CHECK(row.find("recording") != std::string::npos);  // the filter applies to exports
    CHECK_FALSE(std::getline(in, row));
}

TEST_CASE("journal service: an unusable file leaves the app running, with a reason") {
    const TempDir dir;
    fs::create_directories(dir.path / "journal.sqlite");  // a folder where the file should be
    store::JournalService service(dir.path / "journal.sqlite");
    service.add(event(store::EventKind::Pass, "x", 0min));
    service.flush();
    const auto snapshot = service.snapshot();
    CHECK_FALSE(snapshot.open);
    CHECK_FALSE(snapshot.error.empty());
}
