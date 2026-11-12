// SPDX-License-Identifier: GPL-3.0-or-later

#include "integrations/SatDumpRunner.h"

#include "device/IqMetadata.h"
#include "platform/Process.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>

namespace zarescope::integrations {

namespace fs = std::filesystem;

std::vector<std::string> satDumpArguments(const SatDumpJob& job, const fs::path& outputDir) {
    std::vector<std::string> args{job.pipeline,
                                  "baseband",
                                  job.baseband.string(),
                                  outputDir.string(),
                                  "--samplerate",
                                  std::to_string(job.sampleRateSps),
                                  "--baseband_format",
                                  "cu8"};
    args.insert(args.end(), job.extraArgs.begin(), job.extraArgs.end());
    return args;
}

std::optional<float> parseProgress(std::string_view line) {
    constexpr std::string_view kKey = "Progress ";
    const size_t at = line.find(kKey);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    // Digits, optionally a point and more digits, then '%'. Parsed by hand:
    // float std::from_chars needs macOS 26, and strtod follows the locale.
    const std::string_view rest = line.substr(at + kKey.size());
    double value = 0.0;
    double scale = 0.0;  // 0 before the point, then 0.1, 0.01, …
    size_t i = 0;
    for (; i < rest.size(); ++i) {
        const char c = rest[i];
        if (c >= '0' && c <= '9') {
            if (scale == 0.0) {
                value = value * 10.0 + (c - '0');
            } else {
                value += (c - '0') * scale;
                scale /= 10.0;
            }
        } else if (c == '.' && scale == 0.0 && i > 0) {
            scale = 0.1;
        } else {
            break;
        }
    }
    if (i == 0 || i >= rest.size() || rest[i] != '%' || value > 100.0) {
        return std::nullopt;  // also "nan%" and "inf%"
    }
    return static_cast<float>(value);
}

std::optional<std::string> parseVersion(std::string_view line) {
    constexpr std::string_view kKey = "Starting SatDump v";
    const size_t at = line.find(kKey);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    std::string_view rest = line.substr(at + kKey.size());
    const size_t end = rest.find_first_not_of("0123456789.");
    rest = rest.substr(0, end);
    if (rest.empty()) {
        return std::nullopt;
    }
    return std::string(rest);
}

std::string stripAnsi(std::string_view line) {
    std::string out;
    out.reserve(line.size());
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\x1b' && i + 1 < line.size() && line[i + 1] == '[') {
            size_t j = i + 2;
            while (j < line.size() &&
                   (line[j] == ';' || std::isdigit(static_cast<unsigned char>(line[j])) != 0)) {
                ++j;
            }
            if (j < line.size() && line[j] == 'm') {
                i = j;  // skip the whole escape
                continue;
            }
        }
        out += line[i];
    }
    return out;
}

SatDumpRunner::SatDumpRunner(std::vector<fs::path> candidates)
    : m_candidates(std::move(candidates)) {
    m_worker = std::thread([this] { run(); });
}

SatDumpRunner::~SatDumpRunner() {
    {
        const std::scoped_lock lock(m_mutex);
        m_quit = true;
    }
    m_cancel.store(true);
    m_wake.notify_all();
    m_worker.join();
}

void SatDumpRunner::submit(SatDumpJob job, Done done) {
    {
        const std::scoped_lock lock(m_mutex);
        if (!m_quit) {
            m_jobs.emplace_back(std::move(job), std::move(done));
            m_wake.notify_all();
            return;
        }
    }
    // Shutting down: still exactly one call.
    SatDumpResult result;
    result.error = "cancelled";
    done(result);
}

void SatDumpRunner::probe() {
    {
        const std::scoped_lock lock(m_mutex);
        m_probeRequested = true;
    }
    m_wake.notify_all();
}

SatDumpStatus SatDumpRunner::status() const {
    const std::scoped_lock lock(m_mutex);
    return m_status;
}

void SatDumpRunner::waitIdle() {
    std::unique_lock lock(m_mutex);
    m_idle.wait(lock, [this] { return (m_jobs.empty() && !m_busy && !m_probeRequested) || m_quit; });
}

void SatDumpRunner::run() {
    std::unique_lock lock(m_mutex);
    while (true) {
        m_wake.wait(lock, [this] { return m_quit || m_probeRequested || !m_jobs.empty(); });
        if (m_quit) {
            break;
        }
        if (m_probeRequested) {
            m_probeRequested = false;
            lock.unlock();
            probeNow();
            lock.lock();
        }
        if (!m_jobs.empty()) {
            auto [job, done] = std::move(m_jobs.front());
            m_jobs.pop_front();
            m_busy = true;
            m_status.running = true;
            m_status.progressPercent = 0.0F;
            m_status.lastLine.clear();
            lock.unlock();
            const SatDumpResult result = execute(job);
            done(result);
            lock.lock();
            m_busy = false;
            m_status.running = false;
        }
        m_idle.notify_all();
    }
    // Shutting down: every job still waiting gets its one call.
    auto pending = std::move(m_jobs);
    m_jobs.clear();
    lock.unlock();
    for (auto& [job, done] : pending) {
        SatDumpResult result;
        result.error = "cancelled";
        done(result);
    }
    m_idle.notify_all();
}

void SatDumpRunner::probeNow() {
    std::optional<fs::path> found;
    for (const fs::path& candidate : m_candidates) {
        std::error_code ec;
        const fs::path real = fs::canonical(candidate, ec);
        if (ec || !fs::is_regular_file(real, ec)) {
            continue;
        }
        const auto perms = fs::status(real, ec).permissions();
        if (!ec && (perms & fs::perms::owner_exec) != fs::perms::none) {
            found = real;
            break;
        }
    }
    const std::scoped_lock lock(m_mutex);
    m_status.probed = true;
    m_status.executable = found;
}

SatDumpResult SatDumpRunner::execute(const SatDumpJob& job) {
    SatDumpResult result;
    result.productsDir = job.productsDir;
    std::optional<fs::path> executable;
    {
        const std::scoped_lock lock(m_mutex);
        executable = m_status.executable;
    }
    if (!executable) {
        probeNow();  // installed since the last look?
        const std::scoped_lock lock(m_mutex);
        executable = m_status.executable;
    }
    if (!executable) {
        result.error = "SatDump isn't installed";
        return result;
    }

    std::error_code ec;
    fs::create_directories(job.productsDir, ec);
    if (ec) {
        result.error = "cannot create " + job.productsDir.string() + ": " + ec.message();
        return result;
    }
    // SatDump's own output (soft symbols, frames, products) goes to a
    // scratch folder next to the recording, on the same disk.
    const fs::path work = job.baseband.parent_path() / (job.baseband.stem().string() + ".satdump");
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);
    if (ec) {
        result.error = "cannot create " + work.string() + ": " + ec.message();
        return result;
    }

    result.log = job.productsDir / "satdump.log";
    std::ofstream log(result.log, std::ios::binary | std::ios::trunc);
    std::string version;
    const auto onLine = [&](std::string_view raw) {
        const std::string line = stripAnsi(raw);
        log << line << '\n';
        if (version.empty()) {
            if (auto v = parseVersion(line)) {
                version = *v;
            }
        }
        const auto progress = parseProgress(line);
        const std::scoped_lock lock(m_mutex);
        if (progress) {
            m_status.progressPercent = *progress;
        }
        m_status.lastLine = line;
        if (!version.empty()) {
            m_status.version = version;
        }
    };
    const platform::ProcessResult process =
        platform::runProcess(*executable, satDumpArguments(job, work), work, job.timeout, onLine,
                             m_cancel);
    log.close();
    result.exitCode = process.exitCode;
    result.timedOut = process.timedOut;

    // Copy every PNG, keeping SatDump's sub-folders (MSU-MR/, …).
    for (auto it = fs::recursive_directory_iterator(work, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec) || it->path().extension() != ".png") {
            continue;
        }
        const fs::path relative = fs::relative(it->path(), work, ec);
        const fs::path target = job.productsDir / relative;
        std::error_code copyError;
        fs::create_directories(target.parent_path(), copyError);
        fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, copyError);
        if (!copyError) {
            result.images.push_back(target);
        }
    }
    std::sort(result.images.begin(), result.images.end());
    result.ok = !result.images.empty();

    if (!process.started) {
        result.error = process.error;
    } else if (process.timedOut) {
        result.error = "SatDump took longer than " + std::to_string(job.timeout.count()) + " s";
    } else if (process.cancelled) {
        result.error = "cancelled";
    } else if (!result.ok && process.exitCode != 0) {
        result.error = "SatDump failed (exit code " + std::to_string(process.exitCode) + ")";
    } else if (!result.ok) {
        result.error = "SatDump decoded no image (the signal was probably too weak)";
    }

    // pass.json: what was recorded and what came out of it.
    nlohmann::ordered_json info = job.passInfo;
    nlohmann::ordered_json decode;
    decode["satdumpVersion"] = version;
    decode["pipeline"] = job.pipeline;
    decode["exitCode"] = result.exitCode;
    decode["timedOut"] = result.timedOut;
    decode["error"] = result.error;
    nlohmann::ordered_json images = nlohmann::ordered_json::array();
    for (const fs::path& image : result.images) {
        images.push_back(fs::relative(image, job.productsDir, ec).generic_string());
    }
    decode["images"] = images;
    info["decode"] = decode;
    std::ofstream(job.productsDir / "pass.json", std::ios::binary | std::ios::trunc)
        << info.dump(2) << '\n';

    fs::remove_all(work, ec);
    if (result.ok && !job.keepBaseband) {
        fs::remove(job.baseband, ec);
        fs::remove(device::sidecarPath(job.baseband), ec);
    }
    return result;
}

}  // namespace zarescope::integrations
