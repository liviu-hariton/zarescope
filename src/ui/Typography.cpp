// SPDX-License-Identifier: GPL-3.0-or-later
//
// Font loading and the type scale. See Typography.h.

#include "ui/Typography.h"

namespace zarescope::ui {

namespace {

// Positions in kFontFiles.
constexpr size_t kUiRegularIndex = 0;
constexpr size_t kUiMediumIndex = 1;
constexpr size_t kMonoRegularIndex = 3;
constexpr size_t kMonoMediumIndex = 4;

// U+2009 thin space and U+202F narrow no-break space, as an ImGui glyph-range
// list (pairs of first/last, zero-terminated). ImGui keeps a pointer to it, so
// it must live as long as the font atlas: static storage.
constexpr std::array<ImWchar, 5> kThinSpaces{0x2009, 0x2009, 0x202F, 0x202F, 0};

ImFont* pickFont(const FontSet& fonts, const theme::TypeStyle& style, std::string_view role) {
    using Family = theme::TypeStyle::Family;
    if (style.family == Family::Ui) {
        switch (style.weight) {
        case 400: return fonts.uiRegular;
        case 500: return fonts.uiMedium;
        case 600: return fonts.uiSemiBold;
        default: break;
        }
    } else {
        switch (style.weight) {
        case 400: return fonts.monoRegular;
        case 500: return fonts.monoMedium;
        default: break;
        }
    }
    throw theme::TokenError("type style \"" + std::string(role) + "\" asks for weight " +
                            std::to_string(style.weight) + ", which isn't in resources/fonts");
}

}  // namespace

std::string_view tokenName(TypeRole role) {
    switch (role) {
    case TypeRole::Display: return "display";
    case TypeRole::Title: return "title";
    case TypeRole::Heading: return "heading";
    case TypeRole::Body: return "body";
    case TypeRole::Label: return "label";
    case TypeRole::Caption: return "caption";
    case TypeRole::MonoFreq: return "mono/freq";
    case TypeRole::MonoFreqCompact: return "mono/freq-compact";
    case TypeRole::MonoData: return "mono/data";
    case TypeRole::MonoSmall: return "mono/small";
    }
    return "body";
}

std::optional<FontSet> loadFonts(ImFontAtlas& atlas, const std::filesystem::path& fontsDir,
                                 std::string& error) {
    std::array<ImFont*, kFontFiles.size()> loaded{};
    for (size_t i = 0; i < kFontFiles.size(); ++i) {
        const std::filesystem::path file = fontsDir / std::filesystem::path(kFontFiles[i]);
        ImFontConfig config;
        // Keep the font's real advance widths; snapping would undo Inter's
        // spacing at small sizes.
        config.PixelSnapH = false;
        loaded[i] = atlas.AddFontFromFileTTF(file.string().c_str(), 0.0F, &config);
        if (loaded[i] == nullptr) {
            error = "cannot load font " + file.string();
            return std::nullopt;
        }

        // JetBrains Mono has no thin spaces, which the design's number
        // formats need ("137.900 000 MHz", "36 000 ft"). Borrow just those
        // two glyphs from Inter of the same weight (docs/DECISIONS.md).
        if (i == kMonoRegularIndex || i == kMonoMediumIndex) {
            const size_t donor = i == kMonoRegularIndex ? kUiRegularIndex : kUiMediumIndex;
            const std::filesystem::path donorFile =
                fontsDir / std::filesystem::path(kFontFiles[donor]);
            ImFontConfig merge;
            merge.MergeMode = true;
            merge.PixelSnapH = false;
            merge.GlyphRanges = kThinSpaces.data();
            if (atlas.AddFontFromFileTTF(donorFile.string().c_str(), 0.0F, &merge) == nullptr) {
                error = "cannot load font " + donorFile.string();
                return std::nullopt;
            }
        }
    }
    return FontSet{loaded[kUiRegularIndex], loaded[kUiMediumIndex], loaded[2],
                   loaded[kMonoRegularIndex], loaded[kMonoMediumIndex]};
}

Typography::Typography(const FontSet& fonts, const theme::DesignTokens& tokens) {
    for (size_t i = 0; i < kTypeRoleCount; ++i) {
        const auto role = static_cast<TypeRole>(i);
        const std::string_view name = tokenName(role);
        const theme::TypeStyle* style = tokens.typeStyle(name);
        if (style == nullptr) {
            throw theme::TokenError("design tokens: missing type style \"" + std::string(name) +
                                    "\"");
        }
        m_faces[i] = Face{pickFont(fonts, *style, name), style->sizePx, style->uppercase};
    }
}

TypeScope::TypeScope(const Typography& typography, TypeRole role) {
    const auto& face = typography.face(role);
    ImGui::PushFont(face.font, face.sizePx);
}

TypeScope::~TypeScope() { ImGui::PopFont(); }

}  // namespace zarescope::ui
