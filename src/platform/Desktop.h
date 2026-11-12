// SPDX-License-Identifier: GPL-3.0-or-later
//
// Desktop: small things the operating system does for the app.
//
//   revealInFileManager  shows a file or folder in Finder (the decoded images)
//   KeepAwake            tells the OS the app has work to do on its own:
//                        Waiting  automatic recording is on: no App Nap, so
//                                 timers keep firing in the background
//                        Pass     a pass is being recorded or decoded: the
//                                 Mac also doesn't go to sleep when idle
//                        The display may still sleep; a Mac the user puts to
//                        sleep (or closes the lid on) still misses passes.

#pragma once

#include <filesystem>
#include <memory>

namespace zarescope::platform {

void revealInFileManager(const std::filesystem::path& path);

class KeepAwake {
public:
    enum class Level { Normal, Waiting, Pass };

    KeepAwake();
    ~KeepAwake();  // back to Normal
    KeepAwake(const KeepAwake&) = delete;
    KeepAwake& operator=(const KeepAwake&) = delete;
    KeepAwake(KeepAwake&&) = delete;
    KeepAwake& operator=(KeepAwake&&) = delete;

    // UI thread. Cheap when the level doesn't change.
    void set(Level level);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    Level m_level = Level::Normal;
};

}  // namespace zarescope::platform
