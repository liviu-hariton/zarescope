// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fonts and the type scale.
//
// Two families ship in resources/fonts (both SIL OFL 1.1):
//   Inter            400 / 500 / 600   every UI label and paragraph
//   JetBrains Mono   400 / 500         frequencies, coordinates, timers, tables
// (the "NL" cut, without ligatures; digits are tabular in both).
//
// Dear ImGui 1.92+ rasterizes glyphs on demand, at the size and pixel density
// they're drawn at. So each font is loaded once, with no fixed size and no
// glyph ranges: Romanian Ș ș Ț ț (U+0218–U+021B), the minus sign and the thin
// space are picked up the first time they appear, and text stays sharp on
// Retina screens without the old "load at 2×, draw at ½" trick.
//
// Widget code never names a font file or a pixel size. It asks for a role
// from the design's type scale: TypeScope scope(typography, TypeRole::Heading).

#pragma once

#include "ui/theme/DesignTokens.h"

#include <imgui.h>

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace zarescope::ui {

// The steps of design/docs/DESIGN.md §3.
enum class TypeRole {
    Display,          // 32/38  onboarding, empty states
    Title,            // 22/28  dialog and page titles
    Heading,          // 17/24  panel and card headings
    Body,             // 15/22  default
    Label,            // 14/18  buttons, field labels, tabs
    Caption,          // 12/16  overlines, column headers (uppercase)
    MonoFreq,         // 40/44  frequency display
    MonoFreqCompact,  // 30/34  frequency in the top bar
    MonoData,         // 14/20  table numbers, coordinates, timers
    MonoSmall,        // 12/16  axis ticks, the status bar
};
inline constexpr size_t kTypeRoleCount = 10;

// The token file's name for a role ("display", "mono/freq"…).
std::string_view tokenName(TypeRole role);

// The five font files, loaded into an atlas.
struct FontSet {
    ImFont* uiRegular = nullptr;    // Inter 400
    ImFont* uiMedium = nullptr;     // Inter 500
    ImFont* uiSemiBold = nullptr;   // Inter 600
    ImFont* monoRegular = nullptr;  // JetBrains Mono 400
    ImFont* monoMedium = nullptr;   // JetBrains Mono 500
};

// The file names loadFonts() expects in the fonts folder.
inline constexpr std::array<std::string_view, 5> kFontFiles{
    "Inter-Regular.ttf",           "Inter-Medium.ttf",           "Inter-SemiBold.ttf",
    "JetBrainsMonoNL-Regular.ttf", "JetBrainsMonoNL-Medium.ttf",
};

// Loads the five fonts from `fontsDir` into `atlas`. The first one (Inter 400)
// becomes ImGui's default font. On failure, returns std::nullopt and says
// which file in `error`.
std::optional<FontSet> loadFonts(ImFontAtlas& atlas, const std::filesystem::path& fontsDir,
                                 std::string& error);

// Maps each role of the type scale to a loaded font and a size.
class Typography {
public:
    struct Face {
        ImFont* font = nullptr;
        float sizePx = 13.0F;
        bool uppercase = false;  // the caller writes caption text in capitals
    };

    // Throws theme::TokenError if the token file lacks a role or asks for a
    // weight that isn't shipped.
    Typography(const FontSet& fonts, const theme::DesignTokens& tokens);

    [[nodiscard]] const Face& face(TypeRole role) const {
        return m_faces[static_cast<size_t>(role)];
    }

private:
    std::array<Face, kTypeRoleCount> m_faces{};
};

// Pushes a role's font and size for the lifetime of the scope.
class TypeScope {
public:
    TypeScope(const Typography& typography, TypeRole role);
    ~TypeScope();
    TypeScope(const TypeScope&) = delete;
    TypeScope& operator=(const TypeScope&) = delete;
    TypeScope(TypeScope&&) = delete;
    TypeScope& operator=(TypeScope&&) = delete;
};

}  // namespace zarescope::ui
