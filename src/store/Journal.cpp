// SPDX-License-Identifier: GPL-3.0-or-later

#include "store/Journal.h"

#include "device/IqMetadata.h"  // ISO 8601 UTC timestamps

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <array>
#include <fstream>

namespace zarescope::store {

namespace {

// Migration N takes the schema from version N to N + 1. Append only: a
// released migration is never edited, because files out there already ran it.
constexpr std::array<std::string_view, Journal::kSchemaVersion> kMigrations{
    // 0 → 1: the events table.
    R"sql(
CREATE TABLE events (
    id       INTEGER PRIMARY KEY,
    time_utc TEXT NOT NULL,             -- ISO 8601, UTC, milliseconds: sorts as text
    kind     TEXT NOT NULL,             -- pass, images, aircraft, recording
    title    TEXT NOT NULL,
    detail   TEXT NOT NULL DEFAULT '',
    path     TEXT NOT NULL DEFAULT '',
    data     TEXT NOT NULL DEFAULT '{}',-- JSON object
    key      TEXT UNIQUE                -- one row per key when set
);
CREATE INDEX events_time ON events (time_utc);
CREATE INDEX events_kind_time ON events (kind, time_utc);
)sql",
};

// A prepared statement that finalises itself.
class Statement {
public:
    Statement(sqlite3* db, std::string_view sql) {
        sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &m_stmt, nullptr);
    }
    ~Statement() { sqlite3_finalize(m_stmt); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&&) = delete;
    Statement& operator=(Statement&&) = delete;

    [[nodiscard]] bool ok() const { return m_stmt != nullptr; }
    void bind(int index, std::string_view text) {
        sqlite3_bind_text(m_stmt, index, text.data(), static_cast<int>(text.size()),
                          SQLITE_TRANSIENT);
    }
    void bindNull(int index) { sqlite3_bind_null(m_stmt, index); }
    void bind(int index, int64_t value) { sqlite3_bind_int64(m_stmt, index, value); }
    int step() { return sqlite3_step(m_stmt); }
    [[nodiscard]] std::string text(int column) const {
        const auto* p = sqlite3_column_text(m_stmt, column);
        return p == nullptr ? std::string() : std::string(reinterpret_cast<const char*>(p));
    }
    [[nodiscard]] int64_t integer(int column) const { return sqlite3_column_int64(m_stmt, column); }

private:
    sqlite3_stmt* m_stmt = nullptr;
};

bool exec(sqlite3* db, const std::string& sql, std::string& error) {
    char* message = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &message) != SQLITE_OK) {
        error = message != nullptr ? message : "SQLite error";
        sqlite3_free(message);
        return false;
    }
    return true;
}

int userVersion(sqlite3* db) {
    Statement statement(db, "PRAGMA user_version");
    return statement.ok() && statement.step() == SQLITE_ROW
               ? static_cast<int>(statement.integer(0))
               : -1;
}

// LIKE pattern for "contains", with % _ and the escape character escaped.
std::string containsPattern(std::string_view text) {
    std::string pattern = "%";
    for (const char c : text) {
        if (c == '%' || c == '_' || c == '\\') {
            pattern += '\\';
        }
        pattern += c;
    }
    return pattern + "%";
}

std::string csvField(std::string_view value) {
    std::string out = "\"";
    for (const char c : value) {
        out += c;
        if (c == '"') {
            out += '"';
        }
    }
    return out + "\"";
}

}  // namespace

std::string_view kindName(EventKind kind) {
    switch (kind) {
        case EventKind::Pass: return "pass";
        case EventKind::Images: return "images";
        case EventKind::Aircraft: return "aircraft";
        case EventKind::Recording: return "recording";
    }
    return "pass";
}

std::optional<EventKind> parseKind(std::string_view name) {
    for (const EventKind kind :
         {EventKind::Pass, EventKind::Images, EventKind::Aircraft, EventKind::Recording}) {
        if (kindName(kind) == name) {
            return kind;
        }
    }
    return std::nullopt;
}

std::unique_ptr<Journal> Journal::open(const std::filesystem::path& file, std::string& error) {
    std::error_code ec;
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path(), ec);
    }
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(file.string().c_str(), &db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        error = "cannot open " + file.string() + ": " + (db ? sqlite3_errmsg(db) : "out of memory");
        sqlite3_close(db);
        return nullptr;
    }
    std::unique_ptr<Journal> journal(new Journal(db));
    sqlite3_busy_timeout(db, 2000);
    exec(db, "PRAGMA journal_mode = WAL", error);  // fine to fail (e.g. read-only media)
    error.clear();

    const int version = userVersion(db);
    if (version < 0) {
        error = file.filename().string() + " is not a readable SQLite database";
        return nullptr;
    }
    if (version > kSchemaVersion) {
        error = file.filename().string() + " was made by a newer Zarescope (schema " +
                std::to_string(version) + "); it is left untouched";
        return nullptr;
    }
    for (int step = version; step < kSchemaVersion; ++step) {
        const std::string sql = "BEGIN;\n" + std::string(kMigrations[step]) +
                                "\nPRAGMA user_version = " + std::to_string(step + 1) + ";\nCOMMIT;";
        if (!exec(db, sql, error)) {
            exec(db, "ROLLBACK", error);
            error = "migrating the journal to schema " + std::to_string(step + 1) + " failed";
            return nullptr;
        }
    }
    return journal;
}

Journal::~Journal() { sqlite3_close(m_db); }

int Journal::schemaVersion() const { return userVersion(m_db); }

bool Journal::add(const JournalEvent& event, std::string& error) {
    Statement statement(m_db, R"sql(
INSERT INTO events (time_utc, kind, title, detail, path, data, key)
VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
ON CONFLICT (key) DO UPDATE SET
    title = excluded.title, detail = excluded.detail,
    path = excluded.path, data = excluded.data
)sql");
    if (!statement.ok()) {
        error = sqlite3_errmsg(m_db);
        return false;
    }
    statement.bind(1, device::formatUtcTimestamp(event.timeUtc));
    statement.bind(2, kindName(event.kind));
    statement.bind(3, event.title);
    statement.bind(4, event.detail);
    statement.bind(5, event.path);
    statement.bind(6, event.data.empty() ? std::string_view("{}") : std::string_view(event.data));
    if (event.key.empty()) {
        statement.bindNull(7);
    } else {
        statement.bind(7, event.key);
    }
    if (statement.step() != SQLITE_DONE) {
        error = sqlite3_errmsg(m_db);
        return false;
    }
    return true;
}

std::vector<JournalEvent> Journal::query(const JournalFilter& filter, std::string& error) const {
    std::string sql =
        "SELECT id, time_utc, kind, title, detail, path, data, COALESCE(key, '') FROM events "
        "WHERE 1";
    if (filter.kind) {
        sql += " AND kind = ?1";
    }
    if (!filter.text.empty()) {
        sql += " AND (title LIKE ?2 ESCAPE '\\' OR detail LIKE ?2 ESCAPE '\\')";
    }
    if (filter.from) {
        sql += " AND time_utc >= ?3";
    }
    if (filter.to) {
        sql += " AND time_utc < ?4";
    }
    sql += " ORDER BY time_utc DESC, id DESC LIMIT ?5";

    Statement statement(m_db, sql);
    if (!statement.ok()) {
        error = sqlite3_errmsg(m_db);
        return {};
    }
    if (filter.kind) {
        statement.bind(1, kindName(*filter.kind));
    }
    if (!filter.text.empty()) {
        statement.bind(2, containsPattern(filter.text));
    }
    if (filter.from) {
        statement.bind(3, device::formatUtcTimestamp(*filter.from));
    }
    if (filter.to) {
        statement.bind(4, device::formatUtcTimestamp(*filter.to));
    }
    statement.bind(5, static_cast<int64_t>(filter.limit));

    std::vector<JournalEvent> events;
    int rc = 0;
    while ((rc = statement.step()) == SQLITE_ROW) {
        JournalEvent event;
        event.id = statement.integer(0);
        event.timeUtc = device::parseUtcTimestamp(statement.text(1)).value_or(Clock::time_point{});
        event.kind = parseKind(statement.text(2)).value_or(EventKind::Pass);
        event.title = statement.text(3);
        event.detail = statement.text(4);
        event.path = statement.text(5);
        event.data = statement.text(6);
        event.key = statement.text(7);
        events.push_back(std::move(event));
    }
    if (rc != SQLITE_DONE) {
        error = sqlite3_errmsg(m_db);
    }
    return events;
}

std::string toCsv(const std::vector<JournalEvent>& events) {
    std::string out = "time_utc,kind,title,detail,path,data\r\n";
    for (const JournalEvent& e : events) {
        out += csvField(device::formatUtcTimestamp(e.timeUtc)) + "," + csvField(kindName(e.kind)) +
               "," + csvField(e.title) + "," + csvField(e.detail) + "," + csvField(e.path) + "," +
               csvField(e.data) + "\r\n";
    }
    return out;
}

std::string toJson(const std::vector<JournalEvent>& events) {
    nlohmann::ordered_json array = nlohmann::ordered_json::array();
    for (const JournalEvent& e : events) {
        nlohmann::ordered_json item;
        item["timeUtc"] = device::formatUtcTimestamp(e.timeUtc);
        item["kind"] = kindName(e.kind);
        item["title"] = e.title;
        item["detail"] = e.detail;
        item["path"] = e.path;
        auto data = nlohmann::ordered_json::parse(e.data, nullptr, false);
        item["data"] = data.is_discarded() ? nlohmann::ordered_json::object() : data;
        array.push_back(std::move(item));
    }
    return array.dump(2) + "\n";
}

// --- JournalService ------------------------------------------------------------------

JournalService::JournalService(std::filesystem::path file) : m_file(std::move(file)) {
    m_worker = std::thread([this] { run(); });
    post([this] {
        std::string error;
        m_journal = Journal::open(m_file, error);
        {
            const std::scoped_lock lock(m_mutex);
            m_snapshot.open = m_journal != nullptr;
            m_snapshot.error = error;
        }
        refresh();
    });
}

JournalService::~JournalService() {
    {
        const std::scoped_lock lock(m_mutex);
        m_quit = true;
    }
    m_wake.notify_all();
    m_worker.join();
}

void JournalService::post(std::function<void()> job) {
    {
        const std::scoped_lock lock(m_mutex);
        m_jobs.push_back(std::move(job));
    }
    m_wake.notify_all();
}

void JournalService::add(JournalEvent event) {
    post([this, event = std::move(event)] {
        if (!m_journal) {
            return;
        }
        std::string error;
        if (!m_journal->add(event, error)) {
            const std::scoped_lock lock(m_mutex);
            m_snapshot.error = error;
        }
        refresh();
    });
}

void JournalService::setFilter(JournalFilter filter) {
    post([this, filter = std::move(filter)] {
        m_filter = filter;
        refresh();
    });
}

void JournalService::exportTo(std::filesystem::path file, bool json) {
    post([this, file = std::move(file), json] {
        std::string message;
        if (!m_journal) {
            message = "the journal isn't available";
        } else {
            std::string error;
            JournalFilter all = m_filter;  // what is shown, without the row limit
            all.limit = static_cast<size_t>(-1) >> 1;
            const auto events = m_journal->query(all, error);
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << (json ? toJson(events) : toCsv(events));
            message = (out && error.empty()) ? file.string()
                                             : "cannot write " + file.string() + " " + error;
        }
        const std::scoped_lock lock(m_mutex);
        m_snapshot.exportMessage = message;
        ++m_snapshot.exportGeneration;
    });
}

JournalService::Snapshot JournalService::snapshot() const {
    const std::scoped_lock lock(m_mutex);
    return m_snapshot;
}

void JournalService::flush() {
    std::unique_lock lock(m_mutex);
    m_idle.wait(lock, [this] { return m_jobs.empty() && !m_busy; });
}

void JournalService::refresh() {
    if (!m_journal) {
        return;
    }
    std::string error;
    auto events = m_journal->query(m_filter, error);
    const std::scoped_lock lock(m_mutex);
    m_snapshot.events = std::move(events);
    if (!error.empty()) {
        m_snapshot.error = error;
    }
    ++m_snapshot.generation;
}

void JournalService::run() {
    std::unique_lock lock(m_mutex);
    while (true) {
        m_wake.wait(lock, [this] { return m_quit || !m_jobs.empty(); });
        if (m_jobs.empty()) {
            break;  // quitting, and nothing left to do
        }
        auto job = std::move(m_jobs.front());
        m_jobs.pop_front();
        m_busy = true;
        lock.unlock();
        job();
        lock.lock();
        m_busy = false;
        m_idle.notify_all();
    }
    m_journal.reset();
}

}  // namespace zarescope::store
