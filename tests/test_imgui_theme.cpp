// SPDX-License-Identifier: GPL-3.0-or-later
//
// applyTheme(): the token file and this version of Dear ImGui agree on every
// style field and colour slot, and the values land where DESIGN.md says.
// ImGui runs headless here (a context, no window, no frame).

#include "ui/theme/ImGuiTheme.h"

#include <doctest/doctest.h>

using zarescope::theme::DesignTokens;
using zarescope::theme::ThemeVariant;
using zarescope::ui::applyTheme;
using zarescope::ui::toImVec4;

namespace {

// One ImGui context for the duration of a test case.
struct ImGuiContextScope {
    ImGuiContextScope() { ImGui::CreateContext(); }
    ~ImGuiContextScope() { ImGui::DestroyContext(); }
    ImGuiContextScope(const ImGuiContextScope&) = delete;
    ImGuiContextScope& operator=(const ImGuiContextScope&) = delete;
    ImGuiContextScope(ImGuiContextScope&&) = delete;
    ImGuiContextScope& operator=(ImGuiContextScope&&) = delete;
};

bool same(const ImVec4& a, const ImVec4& b) {
    return a.x == doctest::Approx(b.x) && a.y == doctest::Approx(b.y) &&
           a.z == doctest::Approx(b.z) && a.w == doctest::Approx(b.w);
}

}  // namespace

TEST_CASE("theme: every style field and colour slot maps, in both themes") {
    const ImGuiContextScope context;
    const DesignTokens tokens = DesignTokens::fromFile(ZARESCOPE_TOKENS_FILE);
    for (ThemeVariant variant : {ThemeVariant::Dark, ThemeVariant::Light}) {
        CAPTURE(zarescope::theme::variantName(variant));
        ImGuiStyle style;
        const auto report = applyTheme(tokens, variant, style);
        CHECK(report.unknownStyleFields.empty());
        CHECK(report.unknownColorSlots.empty());
        CHECK(report.unmappedColorSlots.empty());
        for (const auto& name : report.unmappedColorSlots) {
            MESSAGE("ImGuiCol_" << name << " has no colour");
        }
    }
}

TEST_CASE("theme: values land where the design says") {
    const ImGuiContextScope context;
    const DesignTokens tokens = DesignTokens::fromFile(ZARESCOPE_TOKENS_FILE);

    ImGuiStyle dark;
    applyTheme(tokens, ThemeVariant::Dark, dark);
    CHECK(same(dark.Colors[ImGuiCol_WindowBg],
               toImVec4(*tokens.color(ThemeVariant::Dark, "bg/base"))));
    CHECK(same(dark.Colors[ImGuiCol_Text],
               toImVec4(*tokens.color(ThemeVariant::Dark, "text/primary"))));
    CHECK(same(dark.Colors[ImGuiCol_CheckMark],
               toImVec4(*tokens.color(ThemeVariant::Dark, "accent"))));
    CHECK(dark.Colors[ImGuiCol_BorderShadow].w == doctest::Approx(0.0));
    CHECK(dark.Colors[ImGuiCol_ModalWindowDimBg].w == doctest::Approx(0.5));
    // A slot the token file doesn't list gets its fallback token.
    CHECK(same(dark.Colors[ImGuiCol_InputTextCursor],
               toImVec4(*tokens.color(ThemeVariant::Dark, "text/primary"))));

    CHECK(dark.FramePadding.x == doctest::Approx(10.0));
    CHECK(dark.FramePadding.y == doctest::Approx(6.0));
    CHECK(dark.FrameRounding == doctest::Approx(6.0));
    CHECK(dark.ChildRounding == doctest::Approx(10.0));
    CHECK(dark.TabBarOverlineSize == doctest::Approx(2.0));
    CHECK(dark.FontSizeBase == doctest::Approx(13.0));

    // Switching themes replaces every colour.
    ImGuiStyle light = dark;
    applyTheme(tokens, ThemeVariant::Light, light);
    CHECK(same(light.Colors[ImGuiCol_WindowBg],
               toImVec4(*tokens.color(ThemeVariant::Light, "bg/base"))));
    CHECK(same(light.Colors[ImGuiCol_NavCursor],
               toImVec4(*tokens.color(ThemeVariant::Light, "focus-ring"))));
    CHECK(light.FramePadding.x == doctest::Approx(10.0));
}
