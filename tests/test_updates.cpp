// SPDX-License-Identifier: GPL-3.0-or-later
//
// The update check (src/app/UpdateCheck.h) and the HTTPS helper it shares
// with the satellite downloads (src/net/HttpGet.h). No network: the checker
// gets a fake download, and httpGet reads file:// URLs.

#include "app/Settings.h"
#include "app/UpdateCheck.h"
#include "net/HttpGet.h"

#include <doctest/doctest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace zarescope;
using app::parseVersion;
using app::SystemClock;
using app::Version;
using std::chrono::hours;
using std::chrono::minutes;

TEST_CASE("updates: version numbers") {
    CHECK(parseVersion("0.10.0") == Version{0, 10, 0});
    CHECK(parseVersion("v1.2.3") == Version{1, 2, 3});
    CHECK(parseVersion("12.0.7") == Version{12, 0, 7});
    for (const char* bad : {"", "1", "1.2", "1.2.3.4", "1.2.x", "1..3", "-1.2.3", "1.2.3-beta",
                            " 1.2.3", "1.2.10000", "+1.2.3", "v"}) {
        CAPTURE(bad);
        CHECK_FALSE(parseVersion(bad).has_value());
    }
    // Numbers, not text: 0.10.0 is newer than 0.9.0.
    CHECK(*parseVersion("0.10.0") > *parseVersion("0.9.0"));
    CHECK(*parseVersion("1.0.0") > *parseVersion("0.99.99"));
    CHECK(*parseVersion("0.10.1") > *parseVersion("0.10.0"));
    CHECK(app::formatVersion({0, 10, 0}) == "0.10.0");
}

TEST_CASE("updates: the answer from zarescope.eu") {
    std::string error;
    CHECK(app::parseLatestAnswer(R"({"version":"0.11.0"})", error) == Version{0, 11, 0});
    CHECK(app::parseLatestAnswer("{\"version\": \"0.11.0\"}\n", error) == Version{0, 11, 0});
    // Extra fields are ignored; only the version is read.
    CHECK(app::parseLatestAnswer(R"({"version":"1.0.0","url":"https://example.com"})", error) ==
          Version{1, 0, 0});
    for (const char* bad : {"", "<html>Not found</html>", "[]", "{}", R"({"version":11})",
                            R"({"version":"0.11"})", R"({"version":null})", R"("0.11.0")"}) {
        CAPTURE(bad);
        error.clear();
        CHECK_FALSE(app::parseLatestAnswer(bad, error).has_value());
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("updates: when an automatic check is due") {
    const auto now = SystemClock::now();
    // Never checked: now.
    CHECK(app::updateCheckDue(std::nullopt, std::nullopt, now));
    // Once a day.
    CHECK_FALSE(app::updateCheckDue(now - hours(23), std::nullopt, now));
    CHECK(app::updateCheckDue(now - hours(24), std::nullopt, now));
    // A check that didn't get through is tried again an hour later, not sooner.
    CHECK_FALSE(app::updateCheckDue(std::nullopt, now - minutes(30), now));
    CHECK(app::updateCheckDue(std::nullopt, now - minutes(61), now));
    CHECK_FALSE(app::updateCheckDue(now - hours(30), now - minutes(5), now));
    // A clock set back: the saved check lies in the future.
    CHECK(app::updateCheckDue(now + hours(48), std::nullopt, now));
    CHECK_FALSE(app::updateCheckDue(now + minutes(10), std::nullopt, now));
}

namespace {

// A fake zarescope.eu: answers `body` (or fails with `error`), counting calls.
struct FakeSite {
    std::string body = R"({"version":"0.11.0"})";
    std::string error;  // non-empty: the download fails
    std::atomic<int> calls{0};
    std::string lastUrl;

    app::UpdateChecker::Fetch fetch() {
        return [this](const std::string& url, std::string& err) -> std::optional<std::string> {
            ++calls;
            lastUrl = url;
            if (!error.empty()) {
                err = error;
                return std::nullopt;
            }
            return body;
        };
    }
};

app::UpdateChecker::Options options(FakeSite& site, std::string current = "0.10.0") {
    app::UpdateChecker::Options o;
    o.currentVersion = std::move(current);
    o.startDelay = std::chrono::milliseconds(0);
    o.fetch = site.fetch();
    return o;
}

}  // namespace

TEST_CASE("updates: a check the user asks for reports its outcome") {
    FakeSite site;
    app::UpdateChecker checker(options(site));

    checker.checkNow();
    checker.waitIdle();
    auto status = checker.status();
    CHECK(site.calls == 1);
    CHECK(site.lastUrl == app::kLatestVersionUrl);
    CHECK(status.manualSerial == 1);
    CHECK(status.manualOutcome == app::UpdateOutcome::Available);
    CHECK(status.available);
    CHECK(status.latestVersion == "0.11.0");
    CHECK(status.lastChecked.has_value());
    CHECK_FALSE(status.checking);

    // The same version as this one: up to date.
    site.body = R"({"version":"0.10.0"})";
    checker.checkNow();
    checker.waitIdle();
    status = checker.status();
    CHECK(status.manualSerial == 2);
    CHECK(status.manualOutcome == app::UpdateOutcome::UpToDate);
    CHECK_FALSE(status.available);

    // An older one announced (a site not yet updated): still up to date.
    site.body = R"({"version":"0.9.0"})";
    checker.checkNow();
    checker.waitIdle();
    CHECK(checker.status().manualOutcome == app::UpdateOutcome::UpToDate);

    // Offline: the reason is reported, and what was known stays.
    const auto before = checker.status().lastChecked;
    site.error = "Couldn't resolve host name";
    checker.checkNow();
    checker.waitIdle();
    status = checker.status();
    CHECK(status.manualOutcome == app::UpdateOutcome::Failed);
    CHECK(status.manualError == "Couldn't resolve host name");
    CHECK(status.latestVersion == "0.9.0");
    CHECK(status.lastChecked == before);

    // A page that isn't the version file.
    site.error.clear();
    site.body = "<html>Not found</html>";
    checker.checkNow();
    checker.waitIdle();
    CHECK(checker.status().manualOutcome == app::UpdateOutcome::Failed);
}

TEST_CASE("updates: nothing is asked while automatic checks are off") {
    FakeSite site;
    {
        app::UpdateChecker checker(options(site));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CHECK(site.calls == 0);
        CHECK_FALSE(checker.status().available);
    }
    CHECK(site.calls == 0);
}

TEST_CASE("updates: automatic checks run once a day, silently") {
    FakeSite site;
    auto o = options(site);
    o.automatic = true;
    app::UpdateChecker checker(std::move(o));
    // Wait for the first one (the start-up delay is 0 here).
    for (int i = 0; i < 200 && !checker.status().lastChecked; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto status = checker.status();
    CHECK(status.lastChecked.has_value());
    CHECK(status.available);
    CHECK(status.manualSerial == 0);  // nothing to report: it wasn't asked for
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(site.calls == 1);  // and not again the same day
}

TEST_CASE("updates: a check from earlier today isn't repeated") {
    FakeSite site;
    auto o = options(site);
    o.automatic = true;
    o.lastChecked = SystemClock::now() - hours(2);
    o.latestVersion = "0.12.0";
    app::UpdateChecker checker(std::move(o));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(site.calls == 0);
    // What the last run found is known at once.
    CHECK(checker.status().available);
    CHECK(checker.status().latestVersion == "0.12.0");
}

TEST_CASE("updates: turning automatic checks on checks at once when due") {
    FakeSite site;
    app::UpdateChecker checker(options(site));
    checker.setAutomatic(true);
    for (int i = 0; i < 200 && site.calls == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(site.calls == 1);
}

TEST_CASE("updates: the settings keep what the check found") {
    app::Settings settings;
    settings.checkUpdates = true;
    settings.latestVersion = "0.11.0";
    settings.updateCheckedUtc = std::chrono::floor<std::chrono::seconds>(SystemClock::now());
    std::string warning;
    const auto back = app::parseSettings(app::settingsToJson(settings), warning);
    CHECK(warning.empty());
    CHECK(back == settings);

    // Defaults: off, never checked.
    const auto defaults = app::parseSettings("{}", warning);
    CHECK_FALSE(defaults.checkUpdates);
    CHECK_FALSE(defaults.updateCheckedUtc.has_value());
    CHECK(defaults.latestVersion.empty());

    // A hand-edited file with nonsense in it: ignored, not fatal.
    const auto odd = app::parseSettings(
        R"({"checkUpdates":true,"latestVersion":"latest","updateCheckedUtc":"yesterday"})",
        warning);
    CHECK(odd.checkUpdates);
    CHECK(odd.latestVersion.empty());
    CHECK_FALSE(odd.updateCheckedUtc.has_value());
}

TEST_CASE("net: httpGet reads small files and refuses big ones") {
    const auto dir = std::filesystem::temp_directory_path() / "zarescope-test-http";
    std::filesystem::create_directories(dir);
    const auto file = dir / "latest.json";
    {
        std::ofstream(file) << R"({"version":"0.11.0"})" << "\n";
    }
    const std::string url = "file://" + file.string();
    std::string error;
    const auto body = net::httpGet(url, "Zarescope-test", 4096, error, true);
    REQUIRE(body.has_value());
    CHECK(body->starts_with(R"({"version":"0.11.0"})"));

    // Larger than allowed: refused, with the reason.
    error.clear();
    CHECK_FALSE(net::httpGet(url, "Zarescope-test", 8, error, true).has_value());
    CHECK(error.find("larger than 8 bytes") != std::string::npos);

    // file:// only when asked for (tests, ZARESCOPE_UPDATE_URL).
    error.clear();
    CHECK_FALSE(net::httpGet(url, "Zarescope-test", 4096, error).has_value());
    CHECK_FALSE(error.empty());
    std::filesystem::remove_all(dir);
}
