// SPDX-License-Identifier: GPL-3.0-or-later
//
// AboutView: "About Zarescope" (Zarescope menu), a small fixed window centred
// over the app, in the macOS manner:
//
//   the logo (full drawing, 80 px), "Zarescope", the version, a faint build
//   line (macOS version, architecture, Dear ImGui) — the two together are what
//   "Copy version info" puts on the clipboard for a bug report, confirmed
//   with "Copied" — one sentence on what the app is, the receive-only note
//   with its check, "Project page"; under a hairline, the licence, the credits
//   and the copyright line in small faint text. Close, Esc or ⌘W close it.
//
// Drawn as a Dear ImGui modal: the rest of the app dims behind it.

#pragma once

#include "ui/UiKit.h"

#include <chrono>
#include <string>

namespace zarescope::ui {

struct AboutInfo {
    std::string version;         // "0.9.0"
    std::string build;           // "macOS 15.1 · arm64 · Dear ImGui 1.93.0"
    std::string copyrightYears;  // "2026", or "2026–2028"
};

class AboutView {
public:
    struct Actions {
        bool openWebsite = false;
    };

    void open() { m_openRequested = true; }
    void close() { m_closeRequested = true; }
    [[nodiscard]] bool isOpen() const { return m_isOpen; }

    // Every frame, at window level.
    Actions draw(UiKit& ui, const AboutInfo& info);

private:
    bool m_openRequested = false;
    bool m_closeRequested = false;
    bool m_isOpen = false;
    std::chrono::steady_clock::time_point m_copiedAt{};
};

}  // namespace zarescope::ui
