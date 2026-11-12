// SPDX-License-Identifier: GPL-3.0-or-later
//
// SatDumpRunner: decodes a pass recording with SatDump's command line, in a
// separate process, on a worker thread of its own (M5).
//
// SatDump isn't part of Zarescope: the user installs it (on macOS, `brew
// install --cask satdump`). Everything here was checked against SatDump
// 1.2.2 on the development machine (docs/DECISIONS.md, 2026-11-12):
//
//   satdump <pipeline> baseband <input.cu8> <output dir>
//           --samplerate <S/s> --baseband_format cu8 [catalog options…]
//
//   - The Meteor-M LRPT pipeline is "meteor_m2-x_lrpt" (72 k symbols/s);
//     it comes from resources/sat_catalog.json, never from code.
//   - SatDump exits with 0 even when it decoded nothing, so success is
//     judged by the PNG images it wrote, not by the exit code.
//   - Progress lines look like "(I) Progress 45.500000%, SNR : 6.2dB, …";
//     the log is coloured with ANSI escapes, stripped before saving.
//
// A job: run SatDump into a scratch folder, copy the PNG images it made into
// the products folder (keeping SatDump's sub-folders, e.g. MSU-MR/), save the
// log there as satdump.log plus a pass.json summary, delete the scratch
// folder and, when images came out, the baseband recording (a 15-minute
// pass is ~1.8 GB; a failed one is kept for another try).
//
// Every job's callback is called exactly once, on the worker thread:
// success, failure, timeout, or the runner shutting down.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace zarescope::integrations {

struct SatDumpJob {
    std::filesystem::path baseband;     // the .cu8 recording (with its sidecar)
    std::filesystem::path productsDir;  // where the images end up (created)
    std::string pipeline;               // e.g. "meteor_m2-x_lrpt"
    uint32_t sampleRateSps = 0;
    std::vector<std::string> extraArgs;  // from the catalog, e.g. "--dc_block"
    std::chrono::seconds timeout{std::chrono::minutes(30)};
    bool keepBaseband = false;  // keep the recording even when images came out
    nlohmann::ordered_json passInfo;  // written into pass.json, results added
};

struct SatDumpResult {
    bool ok = false;  // at least one image came out
    std::string error;
    std::vector<std::filesystem::path> images;  // in productsDir
    std::filesystem::path productsDir;
    std::filesystem::path log;  // productsDir/satdump.log
    int exitCode = -1;
    bool timedOut = false;
};

struct SatDumpStatus {
    bool probed = false;  // the search for SatDump has run
    std::optional<std::filesystem::path> executable;
    std::string version;  // e.g. "1.2.2", from the last run's log
    bool running = false;
    float progressPercent = 0.0F;  // of the current stage
    std::string lastLine;          // the last log line, without colours
};

class SatDumpRunner {
public:
    using Done = std::function<void(const SatDumpResult&)>;

    // `candidates`: where SatDump may be (platform::satDumpCandidates(), or a
    // fake script in tests). The first that exists and is executable wins;
    // links are resolved first (see MacExternalTools.cpp). The search runs
    // on the worker thread, never on the caller's.
    explicit SatDumpRunner(std::vector<std::filesystem::path> candidates);
    ~SatDumpRunner();  // stops a running SatDump; pending jobs fail as "cancelled"
    SatDumpRunner(const SatDumpRunner&) = delete;
    SatDumpRunner& operator=(const SatDumpRunner&) = delete;
    SatDumpRunner(SatDumpRunner&&) = delete;
    SatDumpRunner& operator=(SatDumpRunner&&) = delete;

    // Any thread; returns at once. Jobs run one at a time, in order.
    void submit(SatDumpJob job, Done done);
    // Looks for SatDump again (after the user installed it).
    void probe();

    [[nodiscard]] SatDumpStatus status() const;

    // Blocks until every submitted job has finished. For tests.
    void waitIdle();

private:
    void run();
    void probeNow();
    SatDumpResult execute(const SatDumpJob& job);

    const std::vector<std::filesystem::path> m_candidates;
    mutable std::mutex m_mutex;  // guards everything below
    std::condition_variable m_wake;
    std::condition_variable m_idle;
    std::deque<std::pair<SatDumpJob, Done>> m_jobs;
    bool m_busy = false;
    bool m_probeRequested = true;
    bool m_quit = false;
    SatDumpStatus m_status;
    std::atomic<bool> m_cancel{false};
    std::thread m_worker;
};

// The command-line arguments for a job (after the program name).
std::vector<std::string> satDumpArguments(const SatDumpJob& job,
                                          const std::filesystem::path& outputDir);

// "Progress 45.5%" → 45.5, or std::nullopt for any other line ("nan%"
// and "inf%", which SatDump prints for stages without a size, too).
std::optional<float> parseProgress(std::string_view line);

// "Starting SatDump v1.2.2" → "1.2.2".
std::optional<std::string> parseVersion(std::string_view line);

// The line without ANSI colour escapes (ESC [ … m).
std::string stripAnsi(std::string_view line);

}  // namespace zarescope::integrations
