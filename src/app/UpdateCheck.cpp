// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/UpdateCheck.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>

namespace zarescope::app {

namespace {

constexpr auto kCheckEvery = std::chrono::hours(24);
constexpr auto kRetryAfter = std::chrono::hours(1);
constexpr int kMaxPart = 9999;

}  // namespace

std::optional<Version> parseVersion(std::string_view text) {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) {
        text.remove_prefix(1);
    }
    std::array<int, 3> parts{};
    for (size_t i = 0; i < parts.size(); ++i) {
        const size_t end = i + 1 < parts.size() ? text.find('.') : text.size();
        if (end == std::string_view::npos || end == 0) {
            return std::nullopt;
        }
        const std::string_view digits = text.substr(0, end);
        int value = 0;
        const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (ec != std::errc{} || ptr != digits.data() + digits.size() || value < 0 ||
            value > kMaxPart) {
            return std::nullopt;
        }
        parts[i] = value;
        text.remove_prefix(i + 1 < parts.size() ? end + 1 : end);
    }
    return Version{parts[0], parts[1], parts[2]};
}

std::string formatVersion(const Version& version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor) + "." +
           std::to_string(version.patch);
}

std::optional<Version> parseLatestAnswer(std::string_view body, std::string& error) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(body);
    } catch (const nlohmann::json::parse_error&) {
        error = "the answer is not JSON";
        return std::nullopt;
    }
    const auto it = json.is_object() ? json.find("version") : json.end();
    if (it == json.end() || !it->is_string()) {
        error = "the answer has no version";
        return std::nullopt;
    }
    auto version = parseVersion(it->get<std::string>());
    if (!version) {
        error = "the answer's version is not x.y.z";
    }
    return version;
}

bool updateCheckDue(std::optional<SystemClock::time_point> lastSuccess,
                    std::optional<SystemClock::time_point> lastAttempt,
                    SystemClock::time_point now) {
    if (lastAttempt && now - *lastAttempt < kRetryAfter && now >= *lastAttempt) {
        return false;  // tried within the hour: don't keep knocking
    }
    if (!lastSuccess) {
        return true;
    }
    // A clock that went back (more than the retry time) would otherwise wait
    // until it catches up again.
    if (*lastSuccess - now > kRetryAfter) {
        return true;
    }
    return now - *lastSuccess >= kCheckEvery;
}

UpdateChecker::UpdateChecker(Options options)
    : m_options(std::move(options)), m_current(parseVersion(m_options.currentVersion)) {
    m_status.automatic = m_options.automatic;
    m_status.lastChecked = m_options.lastChecked;
    if (const auto latest = parseVersion(m_options.latestVersion)) {
        m_status.latestVersion = formatVersion(*latest);
        m_status.available = m_current && *latest > *m_current;
    }
    m_worker = std::thread([this] { run(); });
}

UpdateChecker::~UpdateChecker() {
    {
        const std::scoped_lock lock(m_mutex);
        m_quit = true;
    }
    m_wake.notify_all();
    m_worker.join();
}

void UpdateChecker::setAutomatic(bool on) {
    {
        const std::scoped_lock lock(m_mutex);
        if (m_status.automatic == on) {
            return;
        }
        m_status.automatic = on;
        m_poked = true;  // turned on: a first check may be due at once
    }
    m_wake.notify_all();
}

void UpdateChecker::checkNow() {
    {
        const std::scoped_lock lock(m_mutex);
        m_manualRequested = true;
    }
    m_wake.notify_all();
}

UpdateStatus UpdateChecker::status() const {
    const std::scoped_lock lock(m_mutex);
    return m_status;
}

void UpdateChecker::waitIdle() {
    std::unique_lock lock(m_mutex);
    m_idle.wait(lock, [this] { return m_quit || (!m_status.checking && !m_manualRequested); });
}

void UpdateChecker::run() {
    const auto started = std::chrono::steady_clock::now();
    std::unique_lock lock(m_mutex);
    while (!m_quit) {
        m_poked = false;
        const bool warm = std::chrono::steady_clock::now() - started >= m_options.startDelay;
        const bool automaticDue =
            m_status.automatic && warm &&
            updateCheckDue(m_status.lastChecked, m_lastAttempt, SystemClock::now());
        if (m_manualRequested || automaticDue) {
            const bool manual = m_manualRequested;
            m_manualRequested = false;
            m_status.checking = true;
            m_status.checkingManual = manual;
            m_lastAttempt = SystemClock::now();
            lock.unlock();

            std::string error;
            std::optional<Version> latest;
            if (const auto body = m_options.fetch(m_options.url, error)) {
                latest = parseLatestAnswer(*body, error);
            }

            lock.lock();
            m_status.checking = false;
            m_status.checkingManual = false;
            if (latest) {
                m_status.latestVersion = formatVersion(*latest);
                m_status.available = m_current && *latest > *m_current;
                m_status.lastChecked = SystemClock::now();
            }
            if (manual) {
                m_status.manualOutcome = !latest              ? UpdateOutcome::Failed
                                         : m_status.available ? UpdateOutcome::Available
                                                              : UpdateOutcome::UpToDate;
                m_status.manualError = latest ? std::string{} : error;
                ++m_status.manualSerial;
            }
            m_idle.notify_all();
            continue;  // a request may have come in meanwhile
        }
        m_idle.notify_all();
        // Wake for a request at once; otherwise look again within a minute
        // (and right after the start-up delay).
        const auto wait =
            warm ? std::chrono::milliseconds(std::chrono::minutes(1))
                 : m_options.startDelay - std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now() - started);
        m_wake.wait_for(lock, std::max(wait, std::chrono::milliseconds(1)),
                        [this] { return m_quit || m_manualRequested || m_poked; });
    }
    m_idle.notify_all();
}

}  // namespace zarescope::app
