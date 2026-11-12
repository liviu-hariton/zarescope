// SPDX-License-Identifier: GPL-3.0-or-later
//
// Process: runs an external program (SatDump) and waits for it, with a
// timeout, passing every line it prints to a callback.
//
// The program gets its own process group, so a timeout or a cancel stops
// it together with anything it started: first SIGTERM, then, after a grace
// period, SIGKILL. stdout and stderr are merged, in the order printed.
//
// Blocking: call it from a worker thread, never from the UI thread.
// POSIX implementation in platform/posix/ (macOS now, Linux later).

#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace zarescope::platform {

struct ProcessResult {
    bool started = false;  // false: the program couldn't be run at all (see error)
    int exitCode = -1;     // valid when it exited on its own
    bool timedOut = false;
    bool cancelled = false;
    std::string error;
};

// Runs `program` with `arguments` (not including argv[0]) in
// `workingDirectory`. `onLine` gets each output line without its newline.
// `cancel` is polled a few times a second; set it to stop the program.
ProcessResult runProcess(const std::filesystem::path& program,
                         const std::vector<std::string>& arguments,
                         const std::filesystem::path& workingDirectory,
                         std::chrono::seconds timeout,
                         const std::function<void(std::string_view line)>& onLine,
                         const std::atomic<bool>& cancel);

// Where an external tool may be installed on this OS, most likely first.
// macOS, for SatDump: the app bundle in /Applications and ~/Applications,
// then every directory on PATH. The caller checks which ones exist.
std::vector<std::filesystem::path> satDumpCandidates();

}  // namespace zarescope::platform
