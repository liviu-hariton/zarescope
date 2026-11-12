// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fonts and the type scale: the shipped TTFs load, carry the glyphs the
// design needs (DESIGN.md §3, including Romanian comma-below letters), and
// every role of the token file's type scale maps to a shipped weight.

#include "ui/Typography.h"

#include <doctest/doctest.h>

#include <string>

using zarescope::theme::DesignTokens;
using zarescope::ui::FontSet;
using zarescope::ui::loadFonts;
using zarescope::ui::TypeRole;
using zarescope::ui::Typography;

namespace {

struct ImGuiContextScope {
    ImGuiContextScope() { ImGui::CreateContext(); }
    ~ImGuiContextScope() { ImGui::DestroyContext(); }
    ImGuiContextScope(const ImGuiContextScope&) = delete;
    ImGuiContextScope& operator=(const ImGuiContextScope&) = delete;
    ImGuiContextScope(ImGuiContextScope&&) = delete;
    ImGuiContextScope& operator=(ImGuiContextScope&&) = delete;
};

FontSet requireFonts() {
    std::string error;
    auto fonts = loadFonts(*ImGui::GetIO().Fonts, ZARESCOPE_FONTS_DIR, error);
    INFO(error);
    REQUIRE(fonts);
    return *fonts;
}

}  // namespace

TEST_CASE("fonts: the five shipped fonts load") {
    const ImGuiContextScope context;
    const FontSet fonts = requireFonts();
    CHECK(fonts.uiRegular != nullptr);
    CHECK(fonts.uiMedium != nullptr);
    CHECK(fonts.uiSemiBold != nullptr);
    CHECK(fonts.monoRegular != nullptr);
    CHECK(fonts.monoMedium != nullptr);
    // Inter 400 is ImGui's default font.
    CHECK(ImGui::GetIO().Fonts->Fonts[0] == fonts.uiRegular);
}

TEST_CASE("fonts: a missing fonts folder is reported") {
    const ImGuiContextScope context;
    std::string error;
    CHECK_FALSE(loadFonts(*ImGui::GetIO().Fonts, "/nonexistent/fonts", error));
    CHECK(error.find("Inter-Regular.ttf") != std::string::npos);
}

TEST_CASE("fonts: every glyph the design uses is in every font") {
    const ImGuiContextScope context;
    const FontSet fonts = requireFonts();
    // DESIGN.md §3 glyph list, beyond Basic Latin.
    const ImWchar needed[] = {
        0x0218, 0x0219, 0x021A, 0x021B,                  // Ș ș Ț ț, comma below
        0x0102, 0x0103, 0x00C2, 0x00E2, 0x00CE, 0x00EE,  // Ă ă Â â Î î
        0x2009,                                          // thin space (137.900 000 MHz)
        0x202F,                                          // narrow no-break space (36 000 ft)
        0x2212,                                          // minus sign (T−04:32, −42.3 dBFS)
        0x2013, 0x2014,                                  // en and em dash
        0x00B0,                                          // degree sign
        0x2191, 0x2192, 0x2193,                          // arrows
    };
    for (ImFont* font :
         {fonts.uiRegular, fonts.uiMedium, fonts.uiSemiBold, fonts.monoRegular, fonts.monoMedium}) {
        for (ImWchar c : needed) {
            CAPTURE(static_cast<int>(c));
            CHECK(font->IsGlyphInFont(c));
        }
    }
}

TEST_CASE("typography: every role maps to a shipped font and the token size") {
    const ImGuiContextScope context;
    const FontSet fonts = requireFonts();
    const DesignTokens tokens = DesignTokens::fromFile(ZARESCOPE_TOKENS_FILE);
    const Typography typography(fonts, tokens);

    CHECK(typography.face(TypeRole::Body).font == fonts.uiRegular);
    CHECK(typography.face(TypeRole::Body).sizePx == doctest::Approx(15.0));
    CHECK(typography.face(TypeRole::Label).font == fonts.uiMedium);
    CHECK(typography.face(TypeRole::Display).font == fonts.uiSemiBold);
    CHECK(typography.face(TypeRole::Display).sizePx == doctest::Approx(32.0));
    CHECK(typography.face(TypeRole::Caption).uppercase);
    CHECK(typography.face(TypeRole::MonoFreq).font == fonts.monoMedium);
    CHECK(typography.face(TypeRole::MonoFreq).sizePx == doctest::Approx(40.0));
    CHECK(typography.face(TypeRole::MonoSmall).font == fonts.monoRegular);
    for (size_t i = 0; i < zarescope::ui::kTypeRoleCount; ++i) {
        CAPTURE(i);
        CHECK(typography.face(static_cast<TypeRole>(i)).font != nullptr);
    }
}
