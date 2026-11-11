// SPDX-License-Identifier: GPL-3.0-or-later
//
// Turns the design tokens into a Dear ImGui style.
//
// Switching themes calls applyTheme() again with the other variant: every
// colour and size ImGui draws with comes from the token set, so nothing of
// the previous theme is left behind.

#pragma once

#include "ui/theme/DesignTokens.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace zarescope::ui {

ImVec4 toImVec4(const theme::Rgba& rgba);
ImU32 toImU32(const theme::Rgba& rgba);

// What applyTheme() could not map. Empty lists mean the token file and this
// version of ImGui agree completely; the tests require that.
struct ThemeReport {
    std::vector<std::string> unknownStyleFields;  // imgui.style keys ImGuiStyle doesn't have
    std::vector<std::string> unknownColorSlots;   // imgui.colors keys that aren't ImGuiCol names
    std::vector<std::string> unmappedColorSlots;  // ImGuiCol entries nothing gave a colour
};

// Writes the token values into `style`: every ImGuiCol colour, the paddings,
// roundings and border sizes, the base font size and the tab overline.
ThemeReport applyTheme(const theme::DesignTokens& tokens, theme::ThemeVariant variant,
                       ImGuiStyle& style);

}  // namespace zarescope::ui
