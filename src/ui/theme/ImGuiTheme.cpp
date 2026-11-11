// SPDX-License-Identifier: GPL-3.0-or-later
//
// Design tokens → ImGuiStyle. See ImGuiTheme.h.

#include "ui/theme/ImGuiTheme.h"

#include <array>
#include <string_view>
#include <variant>

namespace zarescope::ui {

namespace {

using theme::DesignTokens;
using theme::ThemeVariant;

// The ImGuiStyle fields the token file may set, by the name it uses.
struct StyleField {
    std::string_view name;
    std::variant<float ImGuiStyle::*, ImVec2 ImGuiStyle::*> member;
};

constexpr std::array kStyleFields{
    StyleField{"WindowPadding", &ImGuiStyle::WindowPadding},
    StyleField{"FramePadding", &ImGuiStyle::FramePadding},
    StyleField{"ItemSpacing", &ImGuiStyle::ItemSpacing},
    StyleField{"ItemInnerSpacing", &ImGuiStyle::ItemInnerSpacing},
    StyleField{"CellPadding", &ImGuiStyle::CellPadding},
    StyleField{"IndentSpacing", &ImGuiStyle::IndentSpacing},
    StyleField{"WindowRounding", &ImGuiStyle::WindowRounding},
    StyleField{"ChildRounding", &ImGuiStyle::ChildRounding},
    StyleField{"FrameRounding", &ImGuiStyle::FrameRounding},
    StyleField{"PopupRounding", &ImGuiStyle::PopupRounding},
    StyleField{"GrabRounding", &ImGuiStyle::GrabRounding},
    StyleField{"TabRounding", &ImGuiStyle::TabRounding},
    StyleField{"ScrollbarRounding", &ImGuiStyle::ScrollbarRounding},
    StyleField{"WindowBorderSize", &ImGuiStyle::WindowBorderSize},
    StyleField{"ChildBorderSize", &ImGuiStyle::ChildBorderSize},
    StyleField{"FrameBorderSize", &ImGuiStyle::FrameBorderSize},
    StyleField{"PopupBorderSize", &ImGuiStyle::PopupBorderSize},
    StyleField{"TabBorderSize", &ImGuiStyle::TabBorderSize},
    StyleField{"TabBarBorderSize", &ImGuiStyle::TabBarBorderSize},
    StyleField{"TabBarOverlineSize", &ImGuiStyle::TabBarOverlineSize},
    StyleField{"GrabMinSize", &ImGuiStyle::GrabMinSize},
    StyleField{"ScrollbarSize", &ImGuiStyle::ScrollbarSize},
};

// ImGuiCol entries newer than the token file (v1.0) lists. Each one borrows
// the token of its closest listed neighbour, so the token file stays the only
// place colours are defined. An entry in the token file always wins.
struct ColorFallback {
    std::string_view slot;
    std::string_view token;
};

constexpr std::array kColorFallbacks{
    ColorFallback{"CheckboxSelectedBg",
                  "bg/raised"},  // like FrameBg; the accent check mark stays visible
    ColorFallback{"InputTextCursor", "text/primary"},             // the caret is text
    ColorFallback{"TabDimmedSelectedOverline", "border/strong"},  // selected tab, window unfocused
    ColorFallback{"DockingPreview", "accent/soft"},               // where a dragged panel will land
    ColorFallback{"DockingEmptyBg", "bg/base"},   // an empty dock area is window background
    ColorFallback{"TextLink", "accent"},          // links are interactive, like primary actions
    ColorFallback{"TreeLines", "border/subtle"},  // structure lines, like separators
    ColorFallback{"DragDropTargetBg", "accent/soft"},  // matches DragDropTarget's accent
    ColorFallback{"UnsavedMarker", "text/primary"},    // a dot next to a title
};

void setStyleValue(ImGuiStyle& style, const StyleField& field,
                   const DesignTokens::StyleValue& value) {
    if (std::holds_alternative<float ImGuiStyle::*>(field.member)) {
        style.*std::get<float ImGuiStyle::*>(field.member) = value.front();
    } else {
        const float y = value.size() > 1 ? value[1] : value[0];
        style.*std::get<ImVec2 ImGuiStyle::*>(field.member) = ImVec2(value[0], y);
    }
}

}  // namespace

ImVec4 toImVec4(const theme::Rgba& rgba) { return {rgba.r, rgba.g, rgba.b, rgba.a}; }

ImU32 toImU32(const theme::Rgba& rgba) { return ImGui::ColorConvertFloat4ToU32(toImVec4(rgba)); }

ThemeReport applyTheme(const DesignTokens& tokens, ThemeVariant variant, ImGuiStyle& style) {
    ThemeReport report;

    // Sizes, paddings, roundings, borders.
    for (const auto& [name, value] : tokens.imguiStyle()) {
        bool known = false;
        for (const StyleField& field : kStyleFields) {
            if (field.name == name) {
                setStyleValue(style, field, value);
                known = true;
                break;
            }
        }
        if (!known) {
            report.unknownStyleFields.push_back(name);
        }
    }

    // The design's "horizon rule": a 2-px accent line marks the active tab.
    if (auto focus = tokens.border("focus")) {
        style.TabBarOverlineSize = *focus;
    }
    // Body text is the default (13 px).
    if (const auto* body = tokens.typeStyle("body")) {
        style.FontSizeBase = body->sizePx;
    }

    // Colours: look up every ImGuiCol by its name, first in the token file,
    // then in the fallbacks above.
    for (int slot = 0; slot < ImGuiCol_COUNT; ++slot) {
        const std::string_view name = ImGui::GetStyleColorName(slot);
        std::optional<theme::Rgba> rgba;
        if (auto it = tokens.imguiColors().find(name); it != tokens.imguiColors().end()) {
            rgba = tokens.resolveColor(variant, it->second);
        } else {
            for (const ColorFallback& fallback : kColorFallbacks) {
                if (fallback.slot == name) {
                    rgba = tokens.color(variant, fallback.token);
                    break;
                }
            }
        }
        if (rgba) {
            style.Colors[slot] = toImVec4(*rgba);
        } else {
            report.unmappedColorSlots.emplace_back(name);
        }
    }

    // Names in the token file that this ImGui version doesn't have.
    for (const auto& [name, expression] : tokens.imguiColors()) {
        bool exists = false;
        for (int slot = 0; slot < ImGuiCol_COUNT && !exists; ++slot) {
            exists = name == ImGui::GetStyleColorName(slot);
        }
        if (!exists) {
            report.unknownColorSlots.push_back(name);
        }
    }
    return report;
}

}  // namespace zarescope::ui
