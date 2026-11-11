// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/UiKit.h"

#include "ui/theme/ImGuiTheme.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace zarescope::ui {

namespace {

ImVec2 operator+(ImVec2 a, ImVec2 b) { return {a.x + b.x, a.y + b.y}; }

// DESIGN.md §5: icons in buttons, chips and banners are 16 px.
constexpr float kIconPx = 16.0F;

}  // namespace

UiKit::UiKit(const theme::DesignTokens& tokens, const Typography& typography)
    : m_tokens(tokens), m_typography(typography) {}

void UiKit::beginFrame(theme::ThemeVariant variant, Language language) {
    m_variant = variant;
    m_language = language;
}

// --- Tokens -------------------------------------------------------------------

ImU32 UiKit::color(std::string_view token) const {
    return ImGui::ColorConvertFloat4ToU32(colorVec(token));
}

ImVec4 UiKit::colorVec(std::string_view token) const {
    auto rgba = m_tokens.color(m_variant, token);
    IM_ASSERT(rgba && "unknown colour token");
    return rgba ? toImVec4(*rgba) : ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

ImU32 UiKit::logoColor(std::string_view part) const {
    auto rgba = m_tokens.logoColor(m_variant, part);
    return rgba ? toImU32(*rgba) : color("text/primary");
}

float UiKit::size(std::string_view name) const {
    auto value = m_tokens.size(name);
    IM_ASSERT(value && "unknown size token");
    return value.value_or(0.0F);
}

float UiKit::spacing(std::string_view step) const {
    auto value = m_tokens.spacing(step);
    IM_ASSERT(value && "unknown spacing token");
    return value.value_or(0.0F);
}

float UiKit::radius(std::string_view name) const {
    auto value = m_tokens.radius(name);
    IM_ASSERT(value && "unknown radius token");
    return value.value_or(0.0F);
}

float UiKit::border() const { return m_tokens.border("default").value_or(1.0F); }

std::string_view UiKit::statusToken(Status status) {
    switch (status) {
    case Status::Live: return "live";
    case Status::Recording: return "recording";
    case Status::PassImminent: return "pass-imminent";
    case Status::Success: return "success";
    case Status::Warning: return "warning";
    case Status::Error: return "error";
    case Status::Neutral: break;
    }
    return "text/secondary";
}

std::string UiKit::statusSoftToken(Status status) {
    if (status == Status::Neutral) {
        return "bg/raised";
    }
    return std::string(statusToken(status)) + "/soft";
}

// --- Text ---------------------------------------------------------------------

ImVec2 UiKit::textSize(TypeRole role, std::string_view text, float wrapWidth) const {
    const auto& f = face(role);
    return f.font->CalcTextSizeA(f.sizePx, FLT_MAX, wrapWidth, text.data(),
                                 text.data() + text.size());
}

void UiKit::drawText(ImDrawList& drawList, TypeRole role, ImVec2 pos, ImU32 color,
                     std::string_view text, float wrapWidth) const {
    const auto& f = face(role);
    drawList.AddText(f.font, f.sizePx, pos, color, text.data(), text.data() + text.size(),
                     wrapWidth);
}

float UiKit::drawTextCentered(ImDrawList& drawList, TypeRole role, float x, float top, float height,
                              ImU32 color, std::string_view text) const {
    const ImVec2 size = textSize(role, text);
    drawText(drawList, role, ImVec2(x, top + (height - size.y) * 0.5F), color, text);
    return size.x;
}

// --- Icons --------------------------------------------------------------------

void UiKit::icon(ImDrawList& drawList, Icon icon, ImVec2 c, float size, ImU32 color) const {
    const float r = size * 0.5F;
    const float stroke = std::max(1.5F, size / 12.0F);
    switch (icon) {
    case Icon::None: break;
    case Icon::Dot: drawList.AddCircleFilled(c, r * 0.5F, color); break;
    case Icon::Record: drawList.AddCircleFilled(c, r * 0.6F, color); break;
    case Icon::Stop:
        drawList.AddRectFilled(ImVec2(c.x - r * 0.5F, c.y - r * 0.5F),
                               ImVec2(c.x + r * 0.5F, c.y + r * 0.5F), color, radius("xs"));
        break;
    case Icon::Alert: {
        const ImVec2 top(c.x, c.y - r * 0.85F);
        const ImVec2 left(c.x - r * 0.9F, c.y + r * 0.7F);
        const ImVec2 right(c.x + r * 0.9F, c.y + r * 0.7F);
        drawList.AddTriangle(top, right, left, color, stroke);
        drawList.AddLine(ImVec2(c.x, c.y - r * 0.25F), ImVec2(c.x, c.y + r * 0.2F), color, stroke);
        drawList.AddCircleFilled(ImVec2(c.x, c.y + r * 0.45F), stroke * 0.6F, color);
        break;
    }
    case Icon::Check:
        drawList.AddCircle(c, r * 0.85F, color, 0, stroke);
        drawList.AddLine(ImVec2(c.x - r * 0.4F, c.y), ImVec2(c.x - r * 0.1F, c.y + r * 0.3F), color,
                         stroke);
        drawList.AddLine(ImVec2(c.x - r * 0.1F, c.y + r * 0.3F),
                         ImVec2(c.x + r * 0.4F, c.y - r * 0.3F), color, stroke);
        break;
    case Icon::Settings: {
        // A gear: a ring with eight teeth.
        drawList.AddCircle(c, r * 0.5F, color, 0, stroke);
        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) * 3.14159265F / 4.0F;
            const ImVec2 dir(std::cos(a), std::sin(a));
            drawList.AddLine(ImVec2(c.x + dir.x * r * 0.62F, c.y + dir.y * r * 0.62F),
                             ImVec2(c.x + dir.x * r * 0.88F, c.y + dir.y * r * 0.88F), color,
                             stroke);
        }
        drawList.AddCircleFilled(c, r * 0.18F, color);
        break;
    }
    case Icon::Speaker: {
        // A speaker body and two sound waves.
        const float x0 = c.x - r * 0.8F;
        drawList.AddRectFilled(ImVec2(x0, c.y - r * 0.25F), ImVec2(x0 + r * 0.35F, c.y + r * 0.25F),
                               color);
        drawList.AddTriangleFilled(ImVec2(x0 + r * 0.3F, c.y - r * 0.25F),
                                   ImVec2(x0 + r * 0.8F, c.y - r * 0.65F),
                                   ImVec2(x0 + r * 0.8F, c.y + r * 0.65F), color);
        drawList.AddTriangleFilled(ImVec2(x0 + r * 0.3F, c.y - r * 0.25F),
                                   ImVec2(x0 + r * 0.8F, c.y + r * 0.65F),
                                   ImVec2(x0 + r * 0.3F, c.y + r * 0.25F), color);
        drawList.PathArcTo(ImVec2(c.x + r * 0.05F, c.y), r * 0.45F, -0.8F, 0.8F);
        drawList.PathStroke(color, stroke);
        drawList.PathArcTo(ImVec2(c.x + r * 0.05F, c.y), r * 0.8F, -0.8F, 0.8F);
        drawList.PathStroke(color, stroke);
        break;
    }
    case Icon::ChevronDown:
        drawList.AddLine(ImVec2(c.x - r * 0.4F, c.y - r * 0.15F), ImVec2(c.x, c.y + r * 0.25F),
                         color, stroke);
        drawList.AddLine(ImVec2(c.x, c.y + r * 0.25F), ImVec2(c.x + r * 0.4F, c.y - r * 0.15F),
                         color, stroke);
        break;
    }
}

// --- Buttons --------------------------------------------------------------------

float UiKit::buttonWidth(std::string_view label, Icon icon) const {
    float width = textSize(TypeRole::Label, label).x + 2.0F * spacing("12");
    if (icon != Icon::None) {
        width += kIconPx + spacing("8");  // DESIGN.md §5: 16-px icon, 8-px gap
    }
    return width;
}

bool UiKit::button(const char* id, ImVec2 pos, std::string_view label, ButtonKind kind, Icon icon,
                   bool enabled) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 size(buttonWidth(label, icon), this->size("control"));
    ImGui::SetCursorScreenPos(pos);
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    ImGui::EndDisabled();

    std::string_view bg = "bg/raised";
    std::string_view fg = "text/primary";
    bool outlined = true;
    switch (kind) {
    case ButtonKind::Primary:
        bg = held ? "accent/pressed" : hovered ? "accent/hover" : "accent";
        fg = "on-accent";
        outlined = false;
        break;
    case ButtonKind::Danger:
        bg = "error";
        fg = "on-error";
        outlined = false;
        break;
    case ButtonKind::Secondary:
        bg = held ? "border/strong" : hovered ? "bg/hover" : "bg/raised";
        break;
    }
    if (!enabled) {
        bg = "bg/raised";
        fg = "text/disabled";
        outlined = true;
    }
    const ImVec2 max = pos + size;
    drawList.AddRectFilled(pos, max, color(bg), radius("md"));
    if (outlined) {
        drawList.AddRect(pos, max, color("border/strong"), radius("md"), border());
    }
    float x = pos.x + spacing("12");
    // Leading icons (record, stop…) go before the label; a dropdown's
    // chevron goes after it.
    const bool trailing = icon == Icon::ChevronDown;
    if (icon != Icon::None && !trailing) {
        const ImU32 iconColor = icon == Icon::Record && enabled ? color("recording") : color(fg);
        this->icon(drawList, icon, ImVec2(x + kIconPx * 0.5F, pos.y + size.y * 0.5F), kIconPx,
                   iconColor);
        x += kIconPx + spacing("8");
    }
    x += drawTextCentered(drawList, TypeRole::Label, x, pos.y, size.y, color(fg), label);
    if (trailing) {
        this->icon(drawList, icon, ImVec2(x + spacing("8") + kIconPx * 0.5F, pos.y + size.y * 0.5F),
                   kIconPx, color("text/secondary"));
    }
    return clicked && enabled;
}

bool UiKit::iconButton(const char* id, ImVec2 pos, Icon icon, std::string_view tooltip,
                       bool active) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float side = size("control");
    ImGui::SetCursorScreenPos(pos);
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(side, side));
    if (!tooltip.empty()) {
        ImGui::SetItemTooltip("%s", std::string(tooltip).c_str());
    }
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 max(pos.x + side, pos.y + side);
    drawList.AddRectFilled(pos, max,
                           color(active    ? "accent/soft"
                                 : hovered ? "bg/hover"
                                           : "bg/raised"),
                           radius("md"));
    drawList.AddRect(pos, max, color("border/strong"), radius("md"), border());
    this->icon(drawList, icon, ImVec2(pos.x + side * 0.5F, pos.y + side * 0.5F), kIconPx,
               color(active ? "text/primary" : "text/secondary"));
    return clicked;
}

// --- Chips ----------------------------------------------------------------------

float UiKit::chipWidth(std::string_view label, bool withDot) const {
    float width = textSize(TypeRole::Label, label).x + 2.0F * spacing("12");
    if (withDot) {
        width += 8.0F + spacing("4");
    }
    return width;
}

bool UiKit::chip(const char* id, ImVec2 pos, std::string_view label, Status status, bool clickable,
                 bool withDot) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 size(chipWidth(label, withDot), this->size("controlCompact"));
    bool clicked = false;
    bool hovered = false;
    if (clickable) {
        ImGui::SetCursorScreenPos(pos);
        clicked = ImGui::InvisibleButton(id, size);
        hovered = ImGui::IsItemHovered();
    }
    const ImVec2 max = pos + size;
    const std::string soft = statusSoftToken(status);
    drawList.AddRectFilled(pos, max, color(hovered ? "bg/hover" : soft), radius("pill"));
    drawList.AddRect(pos, max,
                     color(status == Status::Neutral ? "border/subtle" : statusToken(status)),
                     radius("pill"), border());
    float x = pos.x + spacing("12");
    const ImU32 fg = color(status == Status::Neutral ? "text/secondary" : statusToken(status));
    if (withDot) {
        drawList.AddCircleFilled(ImVec2(x + 4.0F, pos.y + size.y * 0.5F), 4.0F, fg);
        x += 8.0F + spacing("4");
    }
    drawTextCentered(drawList, TypeRole::Label, x, pos.y, size.y, fg, label);
    return clicked;
}

// --- Segmented control ------------------------------------------------------------

float UiKit::segmentedWidth(const std::string_view* labels, int count) const {
    float width = 2.0F * spacing("2");
    for (int i = 0; i < count; ++i) {
        width += textSize(TypeRole::Label, labels[i]).x + 2.0F * spacing("12");
    }
    return width;
}

bool UiKit::segmented(const char* id, ImVec2 pos, const std::string_view* labels, int count,
                      int& selected, std::string_view tooltip) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float height = size("control");
    const float inset = spacing("2");
    const float padX = spacing("12");
    const ImVec2 max(pos.x + segmentedWidth(labels, count), pos.y + height);

    drawList.AddRectFilled(pos, max, color("bg/raised"), radius("md"));
    drawList.AddRect(pos, max, color("border/subtle"), radius("md"), border());

    bool changed = false;
    ImGui::PushID(id);
    float x = pos.x + inset;
    for (int i = 0; i < count; ++i) {
        const ImVec2 labelSize = textSize(TypeRole::Label, labels[i]);
        const ImVec2 segMin(x, pos.y + inset);
        const ImVec2 segSize(labelSize.x + 2.0F * padX, height - 2.0F * inset);
        ImGui::SetCursorScreenPos(segMin);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##segment", segSize) && selected != i) {
            selected = i;
            changed = true;
        }
        if (!tooltip.empty()) {
            ImGui::SetItemTooltip("%s", std::string(tooltip).c_str());
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        const bool isSelected = selected == i;
        if (isSelected || hovered) {
            drawList.AddRectFilled(segMin, segMin + segSize,
                                   color(isSelected ? "accent/soft" : "bg/hover"), radius("sm"));
        }
        drawText(drawList, TypeRole::Label,
                 ImVec2(x + padX, segMin.y + (segSize.y - labelSize.y) * 0.5F),
                 color(isSelected || hovered ? "text/primary" : "text/secondary"), labels[i]);
        x += segSize.x;
    }
    ImGui::PopID();
    return changed;
}

// --- Banner -----------------------------------------------------------------------

bool UiKit::banner(const char* id, ImVec2 min, float width, Status status, Icon icon,
                   std::string_view badge, std::string_view message, std::string_view action) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float height = size("recordBanner");
    const ImVec2 max(min.x + width, min.y + height);
    const ImU32 fg = color(statusToken(status));
    drawList.AddRectFilled(min, max, color(statusSoftToken(status)));
    drawList.AddRect(min, max, fg, 0.0F, border());

    const float pad = spacing("16");
    float x = min.x + pad;
    if (icon != Icon::None) {
        this->icon(drawList, icon, ImVec2(x + kIconPx * 0.5F, min.y + height * 0.5F), kIconPx, fg);
        x += kIconPx + spacing("8");
    }
    if (!badge.empty()) {
        x += drawTextCentered(drawList, TypeRole::Label, x, min.y, height, fg, badge) +
             spacing("12");
    }
    bool clicked = false;
    float actionWidth = 0.0F;
    if (!action.empty()) {
        actionWidth = buttonWidth(action);
        clicked =
            button(id, ImVec2(max.x - pad - actionWidth, min.y + (height - size("control")) * 0.5F),
                   action, ButtonKind::Secondary);
    }
    // The message gets what's left; it is clipped rather than overlapping
    // the button (DESIGN.md §7: banners may truncate).
    const float right = max.x - pad - (actionWidth > 0.0F ? actionWidth + spacing("12") : 0.0F);
    drawList.PushClipRect(ImVec2(x, min.y), ImVec2(right, max.y), true);
    drawTextCentered(drawList, TypeRole::Body, x, min.y, height, color("text/primary"), message);
    drawList.PopClipRect();
    return clicked;
}

}  // namespace zarescope::ui
