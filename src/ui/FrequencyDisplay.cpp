// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/FrequencyDisplay.h"

#include "ui/Format.h"

#include <imgui_internal.h>  // SetKeyOwner: keep arrow keys from ImGui's navigation while editing

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace zarescope::ui {

namespace {

constexpr std::string_view kUnit = " MHz";
constexpr int64_t kHighestPlace = 1'000'000'000;  // the GHz digit

int64_t digitAt(int64_t frequencyHz, int64_t place) { return (frequencyHz / place) % 10; }

}  // namespace

float FrequencyDisplay::width(const UiKit& ui, int64_t frequencyHz) const {
    float width = 0.0F;
    for (const auto& glyph : frequencyGlyphs(frequencyHz)) {
        width += ui.textSize(TypeRole::MonoFreqCompact, glyph.text).x;
    }
    return width + ui.textSize(TypeRole::MonoFreqCompact, kUnit).x;
}

float FrequencyDisplay::height(const UiKit& ui) const {
    return ui.textSize(TypeRole::MonoFreqCompact, "0").y;
}

std::optional<int64_t> FrequencyDisplay::draw(UiKit& ui, ImVec2 pos, int64_t frequencyHz,
                                              bool editable, int64_t minHz, int64_t maxHz) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const auto glyphs = frequencyGlyphs(frequencyHz);
    const float lineHeight = height(ui);
    const float underline = ui.tokens().border("focus").value_or(2.0F);
    int64_t target = frequencyHz;
    bool anyHovered = false;

    ImGui::PushID("frequency");
    const ImGuiID ownerId = ImGui::GetID("display");
    float x = pos.x;
    for (size_t i = 0; i < glyphs.size(); ++i) {
        const auto& glyph = glyphs[i];
        const float w = ui.textSize(TypeRole::MonoFreqCompact, glyph.text).x;
        const ImVec2 min(x, pos.y);
        const ImVec2 max(x + w, pos.y + lineHeight);

        if (editable && glyph.placeHz != 0) {
            ImGui::SetCursorScreenPos(min);
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::InvisibleButton("##digit", ImVec2(w, lineHeight))) {
                m_focusedPlace = glyph.placeHz;
            }
            ImGui::SetItemTooltip("%s", std::string(ui.tr(StringId::FrequencyHelp)).c_str());
            if (ImGui::IsItemHovered()) {
                anyHovered = true;
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0F) {
                    target += static_cast<int64_t>(std::lround(wheel)) * glyph.placeHz;
                }
                drawList.AddRectFilled(min, max, ui.color("bg/hover"), ui.radius("xs"));
            }
            ImGui::PopID();
        }
        ui.drawText(drawList, TypeRole::MonoFreqCompact, min,
                    ui.color(glyph.dimmed ? "text/secondary" : "text/primary"), glyph.text);
        if (editable && glyph.placeHz != 0 && glyph.placeHz == m_focusedPlace) {
            drawList.AddRectFilled(ImVec2(min.x, max.y - underline), max, ui.color("accent"));
        }
        x += w;
    }
    ui.drawText(drawList, TypeRole::MonoFreqCompact, ImVec2(x, pos.y), ui.color("text/secondary"),
                kUnit);
    if (!editable) {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##fixed", ImVec2(x - pos.x, lineHeight));
        ImGui::SetItemTooltip("%s", std::string(ui.tr(StringId::FrequencyFixedHelp)).c_str());
        m_focusedPlace = 0;
    }

    // Keyboard editing of the focused digit.
    if (editable && m_focusedPlace != 0) {
        if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !anyHovered) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            m_focusedPlace = 0;
        }
    }
    if (editable && m_focusedPlace != 0) {
        constexpr std::array kArrows{ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow,
                                     ImGuiKey_RightArrow};
        for (ImGuiKey key : kArrows) {
            ImGui::SetKeyOwner(key, ownerId);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, ownerId)) {
            target += m_focusedPlace;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, ownerId)) {
            target -= m_focusedPlace;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, ownerId) &&
            m_focusedPlace < kHighestPlace) {
            m_focusedPlace *= 10;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, ownerId) &&
            m_focusedPlace > 1) {
            m_focusedPlace /= 10;
        }
        for (int d = 0; d <= 9; ++d) {
            const auto key = static_cast<ImGuiKey>(ImGuiKey_0 + d);
            const auto keypad = static_cast<ImGuiKey>(ImGuiKey_Keypad0 + d);
            if (ImGui::IsKeyPressed(key, false) || ImGui::IsKeyPressed(keypad, false)) {
                target += (d - digitAt(target, m_focusedPlace)) * m_focusedPlace;
                if (m_focusedPlace > 1) {
                    m_focusedPlace /= 10;
                }
            }
        }
    }
    ImGui::PopID();

    target = std::clamp(target, minHz, maxHz);
    if (target != frequencyHz) {
        return target;
    }
    return std::nullopt;
}

}  // namespace zarescope::ui
