// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main window's shell (design/docs/DESIGN.md §6), as far as M0 goes:
//
//   ┌ top bar (64) ───────────────────────────────────────────────────────┐
//   │ logo                                   [No receiver]  [Dark|Light]  │
//   ├─────────────────────────────────────────────────────────────────────┤
//   │                                                                     │
//   │                 empty state: no receiver yet                        │
//   │                                                                     │
//   ├ status bar (28) ────────────────────────────────────────────────────┤
//   │ Zarescope 0.1.0                          UTC 14:32:05  Local 16:32:05│
//   └─────────────────────────────────────────────────────────────────────┘
//
// The frequency display, signal meter, tabs, spectrum, waterfall and side
// panel arrive with the milestones that give them data (M1 onwards).

#pragma once

#include "ui/Strings.h"
#include "ui/Typography.h"
#include "ui/theme/DesignTokens.h"

#include <imgui.h>

#include <string>
#include <string_view>

namespace zarescope::ui {

// What the user can change from the shell. The app re-applies the theme
// when `variant` changes.
struct ShellState {
    theme::ThemeVariant variant = theme::ThemeVariant::Dark;
    Language language = Language::English;
};

class MainWindow {
public:
    MainWindow(const theme::DesignTokens& tokens, const Typography& typography,
               std::string versionText);

    // Draws the whole window for one frame. Call between ImGui::NewFrame()
    // and ImGui::Render().
    void draw(ShellState& state);

private:
    // A colour token of the current theme, ready for ImDrawList.
    [[nodiscard]] ImU32 color(std::string_view token) const;
    [[nodiscard]] float sizeToken(std::string_view name) const;
    [[nodiscard]] float spacingToken(std::string_view step) const;
    [[nodiscard]] float radiusToken(std::string_view name) const;
    [[nodiscard]] std::string_view tr(StringId id) const { return text(id, m_state->language); }

    void drawTopBar();
    void drawBody();
    void drawStatusBar();

    // A pill-shaped, read-only status chip. Returns its width.
    float chip(ImVec2 pos, std::string_view label);
    // A segmented control; returns true when the selection changed.
    bool segmented(const char* id, ImVec2 pos, const std::string_view* labels, int count,
                   int& selected);
    [[nodiscard]] float segmentedWidth(const std::string_view* labels, int count) const;
    [[nodiscard]] float chipWidth(std::string_view label) const;

    const theme::DesignTokens& m_tokens;
    const Typography& m_typography;
    std::string m_versionText;
    ShellState* m_state = nullptr;  // valid during draw() only
};

}  // namespace zarescope::ui
