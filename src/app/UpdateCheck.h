// SPDX-License-Identifier: GPL-3.0-or-later
//
// The update check: asks zarescope.eu whether a newer Zarescope exists.
// Off unless the user turns it on (Settings › Updates), or asks for it
// (Zarescope › Check for Updates…). docs/DECISIONS.md, 2026-11-12.
//
// What it does, and nothing more:
//   - a GET of https://zarescope.eu/latest.json, a file the website build
//     writes with the current version: {"version":"0.11.0"}. The request
//     names the app and its version in the User-Agent, the same one CelesTrak
//     sees, and carries nothing about the user, the station or the receiver;
//   - at most 4 KB of answer is read;
//   - when a newer version exists, the app says so and links to its own
//     download page, whose address is fixed in the app: the server's answer
//     is only a version number and can't send anyone elsewhere. Nothing is
//     downloaded or installed.
//
// Automatic checks: once a day when one gets through, 10 s after start-up at
// the earliest; a check that can't get through is tried again an hour later,
// silently. A check the user asks for always runs, and reports its outcome.
//
// The pure parts (versions, the answer, the timing) are free functions, so
// the tests cover them without a network; the worker takes the download as a
// function, so the tests give it a fake one.

#pragma once

#include <chrono>
#include <compare>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace zarescope::app {

using SystemClock = std::chrono::system_clock;

// Where the version file lives, and where people get the app. Fixed here,
// not taken from any answer.
inline constexpr std::string_view kLatestVersionUrl = "https://zarescope.eu/latest.json";
inline constexpr std::string_view kDownloadPageEnglish = "https://zarescope.eu/download/";
inline constexpr std::string_view kDownloadPageRomanian = "https://zarescope.eu/ro/descarca/";
inline constexpr size_t kMaxLatestBytes = 4 * 1024;

// A release number: major.minor.patch, as in CMakeLists.txt.
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    auto operator<=>(const Version&) const = default;
};

// "0.10.0" or "v0.10.0" → {0, 10, 0}. Exactly three numbers, none above
// 9999; anything else (a pre-release suffix, a missing part) is rejected.
std::optional<Version> parseVersion(std::string_view text);
std::string formatVersion(const Version& version);

// latest.json's text → the version it announces, as "x.y.z". `error` says
// what was wrong otherwise.
std::optional<Version> parseLatestAnswer(std::string_view body, std::string& error);

// Whether an automatic check should run now. `lastSuccess` is the last check
// that got through (saved between runs), `lastAttempt` the last one tried
// this run. A clock set back by more than an hour counts as due.
bool updateCheckDue(std::optional<SystemClock::time_point> lastSuccess,
                    std::optional<SystemClock::time_point> lastAttempt,
                    SystemClock::time_point now);

enum class UpdateOutcome { None, UpToDate, Available, Failed };

struct UpdateStatus {
    bool automatic = false;
    bool checking = false;
    bool checkingManual = false;  // … and the user asked for this one
    // The newest version zarescope.eu announced ("" before any answer), and
    // whether it is newer than this one.
    std::string latestVersion;
    bool available = false;
    // When a check last got through.
    std::optional<SystemClock::time_point> lastChecked;
    // The last check the user asked for: a new serial each time one ends.
    uint64_t manualSerial = 0;
    UpdateOutcome manualOutcome = UpdateOutcome::None;
    std::string manualError;
};

class UpdateChecker {
public:
    // Downloads `url`: the body, or std::nullopt with `error`. Blocking.
    using Fetch =
        std::function<std::optional<std::string>(const std::string& url, std::string& error)>;
    struct Options {
        std::string currentVersion;  // "0.10.0"
        std::string url = std::string(kLatestVersionUrl);
        // What was saved last time.
        bool automatic = false;
        std::optional<SystemClock::time_point> lastChecked;
        std::string latestVersion;
        // The first automatic check waits this long after start-up.
        std::chrono::milliseconds startDelay = std::chrono::seconds(10);
        Fetch fetch;  // required
    };

    explicit UpdateChecker(Options options);
    ~UpdateChecker();
    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;
    UpdateChecker(UpdateChecker&&) = delete;
    UpdateChecker& operator=(UpdateChecker&&) = delete;

    // UI thread; return at once.
    void setAutomatic(bool on);
    void checkNow();

    [[nodiscard]] UpdateStatus status() const;
    // Blocks until no check is running or asked for. For tests.
    void waitIdle();

private:
    void run();

    const Options m_options;
    const std::optional<Version> m_current;

    mutable std::mutex m_mutex;  // guards everything below
    std::condition_variable m_wake;
    std::condition_variable m_idle;
    UpdateStatus m_status;
    std::optional<SystemClock::time_point> m_lastAttempt;
    bool m_manualRequested = false;
    bool m_poked = false;  // automatic switched: look again now
    bool m_quit = false;
    std::thread m_worker;
};

}  // namespace zarescope::app
