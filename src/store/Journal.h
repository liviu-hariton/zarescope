// SPDX-License-Identifier: GPL-3.0-or-later
//
// Journal: what was received, kept in SQLite (journal.sqlite in the data
// folder; schema in docs/FORMATS.md).
//
// One table of events, each with a kind:
//   pass       a satellite pass handled by the automation, and its outcome
//   images     the images a pass produced (path = the products folder)
//   aircraft   one row per aircraft per day (key "aircraft:<day>:<ICAO>"),
//              updated as the callsign or last-seen time changes
//   recording  an IQ recording the user made (path = the .cu8)
//
// The schema is versioned with SQLite's user_version: open() applies each
// missing migration in order, in a transaction, and refuses a file made by
// a newer Zarescope rather than damaging it.
//
// Journal itself is synchronous (tests use it directly); JournalService
// runs it on a worker thread for the app, so the UI never waits on SQLite.

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

struct sqlite3;

namespace zarescope::store {

using Clock = std::chrono::system_clock;

enum class EventKind { Pass, Images, Aircraft, Recording };

std::string_view kindName(EventKind kind);  // "pass", "images", "aircraft", "recording"
std::optional<EventKind> parseKind(std::string_view name);

struct JournalEvent {
    int64_t id = 0;  // assigned by the journal
    Clock::time_point timeUtc;
    EventKind kind = EventKind::Pass;
    std::string title;   // e.g. "Meteor-M N2-4"
    std::string detail;  // one line, e.g. "4 images" or "RYR4TG · 4CA1FA"
    std::string path;    // a file or folder, if the event has one
    std::string data;    // JSON object with the details ("{}" if none)
    std::string key;     // non-empty: one row per key, updated in place
};

struct JournalFilter {
    std::optional<EventKind> kind;
    std::string text;  // matches title or detail, case-insensitively
    std::optional<Clock::time_point> from;  // inclusive
    std::optional<Clock::time_point> to;    // exclusive
    size_t limit = 1000;                    // newest first
};

class Journal {
public:
    static constexpr int kSchemaVersion = 1;

    // Opens (creating if needed) and migrates. nullptr + error on failure.
    static std::unique_ptr<Journal> open(const std::filesystem::path& file, std::string& error);
    ~Journal();
    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;
    Journal(Journal&&) = delete;
    Journal& operator=(Journal&&) = delete;

    // Adds an event, or with a key already present, updates that row's
    // title, detail, path and data (its time stays the first one).
    bool add(const JournalEvent& event, std::string& error);
    [[nodiscard]] std::vector<JournalEvent> query(const JournalFilter& filter,
                                                  std::string& error) const;
    [[nodiscard]] int schemaVersion() const;

private:
    explicit Journal(sqlite3* db) : m_db(db) {}
    sqlite3* m_db;
};

// Exports, newest first as queried. CSV follows RFC 4180 (quoted fields,
// CRLF); JSON is an array of objects with `data` embedded as an object.
std::string toCsv(const std::vector<JournalEvent>& events);
std::string toJson(const std::vector<JournalEvent>& events);

// The journal for the app: a worker thread in front of Journal.
class JournalService {
public:
    struct Snapshot {
        bool open = false;
        std::string error;  // why the journal isn't available, or the last failure
        std::vector<JournalEvent> events;  // the latest query's results
        uint64_t generation = 0;           // grows with each new result
        std::string exportMessage;         // the last export: the file written, or why not
        uint64_t exportGeneration = 0;
    };

    explicit JournalService(std::filesystem::path file);
    ~JournalService();
    JournalService(const JournalService&) = delete;
    JournalService& operator=(const JournalService&) = delete;
    JournalService(JournalService&&) = delete;
    JournalService& operator=(JournalService&&) = delete;

    // All return at once (UI thread). Adding re-runs the current query.
    void add(JournalEvent event);
    void setFilter(JournalFilter filter);
    void exportTo(std::filesystem::path file, bool json);

    [[nodiscard]] Snapshot snapshot() const;
    // Blocks until every request so far has run. For tests.
    void flush();

private:
    void post(std::function<void()> job);
    void run();
    void refresh();  // worker thread

    const std::filesystem::path m_file;
    std::unique_ptr<Journal> m_journal;  // worker thread only
    JournalFilter m_filter;              // worker thread only

    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_idle;
    std::deque<std::function<void()>> m_jobs;
    bool m_busy = false;
    bool m_quit = false;
    Snapshot m_snapshot;
    std::thread m_worker;
};

}  // namespace zarescope::store
