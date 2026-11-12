// SPDX-License-Identifier: GPL-3.0-or-later
//
// runProcess() with posix_spawn: a pipe for the merged output, a process
// group of its own for clean stopping, poll() so the timeout and the cancel
// flag are checked even while the program prints nothing.

#include "platform/Process.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <thread>

extern char** environ;  // NOLINT: POSIX names it so

namespace zarescope::platform {

namespace {

constexpr int kPollMs = 200;
constexpr auto kGracePeriod = std::chrono::seconds(5);  // SIGTERM → SIGKILL
constexpr size_t kMaxLineBytes = 64 * 1024;             // longer lines are cut

// Stops the whole process group: politely, then not.
void stopGroup(pid_t pid) {
    ::kill(-pid, SIGTERM);
    const auto deadline = std::chrono::steady_clock::now() + kGracePeriod;
    while (std::chrono::steady_clock::now() < deadline) {
        int status = 0;
        if (::waitpid(pid, &status, WNOHANG) == pid) {
            ::kill(-pid, SIGKILL);  // anything it left behind in the group
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ::kill(-pid, SIGKILL);
}

}  // namespace

ProcessResult runProcess(const std::filesystem::path& program,
                         const std::vector<std::string>& arguments,
                         const std::filesystem::path& workingDirectory,
                         std::chrono::seconds timeout,
                         const std::function<void(std::string_view line)>& onLine,
                         const std::atomic<bool>& cancel) {
    ProcessResult result;

    int pipeFds[2] = {-1, -1};
    if (::pipe(pipeFds) != 0) {
        result.error = std::string("pipe: ") + std::strerror(errno);
        return result;
    }
    ::fcntl(pipeFds[0], F_SETFD, FD_CLOEXEC);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
    posix_spawn_file_actions_addclose(&actions, pipeFds[1]);
    // macOS 10.15+ and glibc 2.29+.
    posix_spawn_file_actions_addchdir_np(&actions, workingDirectory.c_str());

    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);  // its own group, led by itself

    const std::string programText = program.string();
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(programText.c_str()));
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int spawnError =
        ::posix_spawn(&pid, programText.c_str(), &actions, &attributes, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    ::close(pipeFds[1]);
    if (spawnError != 0) {
        ::close(pipeFds[0]);
        result.error = "cannot run " + programText + ": " + std::strerror(spawnError);
        return result;
    }
    result.started = true;

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::string pending;
    char buffer[4096];
    bool open = true;
    while (open) {
        if (cancel.load()) {
            result.cancelled = true;
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            result.timedOut = true;
            break;
        }
        pollfd fd{pipeFds[0], POLLIN, 0};
        const int ready = ::poll(&fd, 1, kPollMs);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready <= 0) {
            continue;  // nothing printed: check the clock and the flag again
        }
        const ssize_t count = ::read(pipeFds[0], buffer, sizeof buffer);
        if (count <= 0) {
            open = false;  // end of output: the program closed its side (exited)
            break;
        }
        pending.append(buffer, static_cast<size_t>(count));
        size_t start = 0;
        for (size_t newline; (newline = pending.find('\n', start)) != std::string::npos;) {
            onLine(std::string_view(pending).substr(start, newline - start));
            start = newline + 1;
        }
        pending.erase(0, start);
        if (pending.size() > kMaxLineBytes) {
            onLine(pending);
            pending.clear();
        }
    }
    ::close(pipeFds[0]);
    if (!pending.empty()) {
        onLine(pending);
    }

    // The output is closed; the program normally exits right after. Keep
    // honouring the timeout and the cancel flag in case it doesn't.
    int status = 0;
    while (!result.timedOut && !result.cancelled) {
        const pid_t done = ::waitpid(pid, &status, WNOHANG);
        if (done == pid || (done < 0 && errno != EINTR)) {
            break;
        }
        result.cancelled = cancel.load();
        result.timedOut = std::chrono::steady_clock::now() >= deadline;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (result.timedOut || result.cancelled) {
        stopGroup(pid);
        ::waitpid(pid, &status, 0);
        return result;
    }
    if (WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.error = "stopped by signal " + std::to_string(WTERMSIG(status));
    }
    return result;
}

}  // namespace zarescope::platform
