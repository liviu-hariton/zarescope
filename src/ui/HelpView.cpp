// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/HelpView.h"

#include <algorithm>
#include <cctype>

namespace zarescope::ui {

namespace {

// About 75 characters of 15-px Inter a line.
constexpr float kMeasurePx = 580.0F;
// The "On this page" rail.
constexpr float kRailPx = 240.0F;
// Label column of label/explanation rows.
constexpr float kLabelPx = 180.0F;
// Bullets and step numbers hang in this indent.
constexpr float kIndentPx = 24.0F;

using Kind = GuideBlock::Kind;

}  // namespace

void HelpView::layout(UiKit& ui, const Guide& guide, float width) {
    m_blockY.assign(guide.blocks.size(), 0.0F);
    m_blockH.assign(guide.blocks.size(), 0.0F);
    const float rowPad = ui.spacing("12");
    float y = ui.spacing("24");
    Kind previous = Kind::Title;
    for (size_t i = 0; i < guide.blocks.size(); ++i) {
        const GuideBlock& block = guide.blocks[i];
        float before = 0.0F;
        float height = 0.0F;
        switch (block.kind) {
        case Kind::Title: height = ui.textSize(TypeRole::Title, block.text, width).y; break;
        case Kind::Section:
            before = ui.spacing("48");
            height = ui.textSize(TypeRole::Heading, block.text, width).y;
            break;
        case Kind::Subsection:
            before = ui.spacing("32");
            height = ui.textSize(TypeRole::Heading, block.text, width).y;
            break;
        case Kind::Paragraph:
            before = previous == Kind::Section || previous == Kind::Subsection ? ui.spacing("12")
                                                                               : ui.spacing("16");
            if (previous == Kind::Title) {
                before = ui.spacing("12");
            }
            height = ui.textSize(TypeRole::Body, block.text, width).y;
            break;
        case Kind::Bullet:
        case Kind::Step: {
            const bool continues = previous == block.kind;
            const bool afterHeading = previous == Kind::Section || previous == Kind::Subsection;
            before = continues ? ui.spacing("8")
                               : (afterHeading ? ui.spacing("12") : ui.spacing("16"));
            height = ui.textSize(TypeRole::Body, block.text, width - kIndentPx).y;
            break;
        }
        case Kind::TableHeader:
            before = previous == Kind::Section ? ui.spacing("12") : ui.spacing("24");
            height = ui.textSize(TypeRole::Caption, "N").y + ui.spacing("8");
            break;
        case Kind::Row: {
            const float textW = width - kLabelPx - ui.spacing("16");
            height = std::max(ui.textSize(TypeRole::Body, block.label, kLabelPx).y,
                              ui.textSize(TypeRole::Body, block.text, textW).y) +
                     2.0F * rowPad;
            break;
        }
        }
        y += before;
        m_blockY[i] = y;
        m_blockH[i] = height;
        y += height;
        previous = block.kind;
    }
    m_contentHeight = y;
}

HelpView::Actions HelpView::draw(UiKit& ui, ImVec2 pos, ImVec2 size, const Guide* guide,
                                 std::string_view footer) {
    Actions actions;
    ImDrawList& outer = *ImGui::GetWindowDrawList();
    if (guide == nullptr || guide->blocks.empty()) {
        const float measure = std::min(kMeasurePx, size.x);
        ui.drawText(outer, TypeRole::Body,
                    ImVec2(pos.x + (size.x - measure) * 0.5F, pos.y + size.y * 0.3F),
                    ui.color("text/secondary"), ui.tr(StringId::HelpUnavailable), measure);
        return actions;
    }

    const float gap = ui.spacing("48");
    const bool withRail = size.x >= kRailPx + gap + kMeasurePx;
    const float width = std::min(kMeasurePx, size.x - (withRail ? kRailPx + gap : 0.0F));
    // The rail and the column, centred together.
    const float block = (withRail ? kRailPx + gap : 0.0F) + width;
    const float left = pos.x + std::max(0.0F, (size.x - block) * 0.5F);
    const float columnX = left + (withRail ? kRailPx + gap : 0.0F);

    if (m_laidOut != guide || m_laidOutWidth != width || m_laidOutLanguage != ui.language()) {
        layout(ui, *guide, width);
        m_laidOut = guide;
        m_laidOutWidth = width;
        m_laidOutLanguage = ui.language();
    }
    if (m_jumpId) {
        m_scrollTo = 0.0F;
        for (const size_t index : guide->sections) {
            if (!m_jumpId->empty() && guide->blocks[index].id == *m_jumpId) {
                m_scrollTo = m_blockY[index] - ui.spacing("24");
            }
        }
        m_jumpId.reset();
    }

    // The scrolling column: a child window spanning the whole height, so the
    // wheel works anywhere over the page.
    const float footerH = ui.spacing("48") + ui.textSize(TypeRole::Caption, "N").y + ui.spacing("32");
    ImGui::SetCursorScreenPos(ImVec2(columnX, pos.y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui.colorVec("bg/base"));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
    ImGui::BeginChild("##helpText", ImVec2(pos.x + size.x - columnX, size.y), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImDrawList& drawList = *ImGui::GetWindowDrawList();

    // Keyboard scrolling, unless a text field has the keyboard.
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput && !ImGui::IsAnyItemActive()) {
        const float line = ui.textSize(TypeRole::Body, "N").y * 2.0F;
        const float pageH = size.y - line;
        float scroll = ImGui::GetScrollY();
        const float before = scroll;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) scroll += line;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) scroll -= line;
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_Space)) scroll += pageH;
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) scroll -= pageH;
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) scroll = 0.0F;
        if (ImGui::IsKeyPressed(ImGuiKey_End)) scroll = ImGui::GetScrollMaxY();
        if (scroll != before) {
            m_scrollTo = scroll;
        }
    }
    if (m_scrollTo) {
        ImGui::SetScrollY(std::clamp(*m_scrollTo, 0.0F, std::max(0.0F, m_contentHeight + footerH - size.y)));
        m_scrollTo.reset();
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();  // already moved by the scroll
    const float scrollY = ImGui::GetScrollY();
    const float rowPad = ui.spacing("12");
    const float bodyLine = ui.textSize(TypeRole::Body, "N").y;
    for (size_t i = 0; i < guide->blocks.size(); ++i) {
        const float top = m_blockY[i];
        if (top + m_blockH[i] < scrollY - bodyLine || top > scrollY + size.y + bodyLine) {
            continue;  // off screen
        }
        const GuideBlock& b = guide->blocks[i];
        const ImVec2 at(origin.x, origin.y + top);
        switch (b.kind) {
        case Kind::Title:
            ui.drawText(drawList, TypeRole::Title, at, ui.color("text/primary"), b.text, width);
            break;
        case Kind::Section:
            ui.drawText(drawList, TypeRole::Heading, at, ui.color("text/primary"), b.text, width);
            break;
        case Kind::Subsection:
            // Same size as a section, in the secondary colour: one step down.
            ui.drawText(drawList, TypeRole::Heading, at, ui.color("text/secondary"), b.text, width);
            break;
        case Kind::Paragraph:
            ui.drawText(drawList, TypeRole::Body, at, ui.color("text/primary"), b.text, width);
            break;
        case Kind::Bullet: {
            const float dot = ui.spacing("4") + ui.spacing("2");
            drawList.AddCircleFilled(ImVec2(at.x + kIndentPx * 0.4F, at.y + bodyLine * 0.5F),
                                     dot * 0.5F, ui.color("text/secondary"));
            ui.drawText(drawList, TypeRole::Body, ImVec2(at.x + kIndentPx, at.y),
                        ui.color("text/primary"), b.text, width - kIndentPx);
            break;
        }
        case Kind::Step:
            ui.drawText(drawList, TypeRole::Body, at, ui.color("text/secondary"),
                        std::to_string(b.number) + ".");
            ui.drawText(drawList, TypeRole::Body, ImVec2(at.x + kIndentPx, at.y),
                        ui.color("text/primary"), b.text, width - kIndentPx);
            break;
        case Kind::TableHeader: {
            std::string label = b.label;
            std::string text = b.text;
            for (std::string* s : {&label, &text}) {
                std::transform(s->begin(), s->end(), s->begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            }
            ui.drawText(drawList, TypeRole::Caption, at, ui.color("text/secondary"), label);
            ui.drawText(drawList, TypeRole::Caption, ImVec2(at.x + kLabelPx + ui.spacing("16"), at.y),
                        ui.color("text/secondary"), text);
            const float lineY = at.y + m_blockH[i] - ui.border();
            drawList.AddRectFilled(ImVec2(at.x, lineY), ImVec2(at.x + width, lineY + ui.border()),
                                   ui.color("border/subtle"));
            break;
        }
        case Kind::Row: {
            ui.drawText(drawList, TypeRole::Body, ImVec2(at.x, at.y + rowPad),
                        ui.color("text/primary"), b.label, kLabelPx);
            const float textX = at.x + kLabelPx + ui.spacing("16");
            ui.drawText(drawList, TypeRole::Body, ImVec2(textX, at.y + rowPad),
                        ui.color("text/secondary"), b.text, at.x + width - textX);
            const float lineY = at.y + m_blockH[i] - ui.border();
            drawList.AddRectFilled(ImVec2(at.x, lineY), ImVec2(at.x + width, lineY + ui.border()),
                                   ui.color("border/subtle"));
            break;
        }
        }
    }

    // The page ends with the version and a way to the website.
    float y = origin.y + m_contentHeight + ui.spacing("48");
    drawList.AddRectFilled(ImVec2(origin.x, y - ui.spacing("16")),
                           ImVec2(origin.x + width, y - ui.spacing("16") + ui.border()),
                           ui.color("border/subtle"));
    const float footerTextW = ui.drawTextCentered(drawList, TypeRole::Caption, origin.x, y,
                                                  ui.size("control"), ui.color("text/secondary"),
                                                  footer);
    const std::string_view website = ui.tr(StringId::MenuWebsite);
    if (ui.button("##helpWebsite", ImVec2(origin.x + footerTextW + ui.spacing("16"), y), website,
                  ButtonKind::Secondary)) {
        actions.openWebsite = true;
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, y + ui.size("control") + ui.spacing("32")));
    ImGui::Dummy(ImVec2(0.0F, 0.0F));
    ImGui::EndChild();

    // On this page: the section being read is the last one whose heading is
    // at or above the top of the view.
    if (withRail) {
        float ry = pos.y + ui.spacing("24");
        ui.drawText(outer, TypeRole::Caption, ImVec2(left, ry), ui.color("text/secondary"),
                    ui.tr(StringId::HelpOnThisPage));
        ry += ui.textSize(TypeRole::Caption, "N").y + ui.spacing("12");
        size_t current = guide->sections.empty() ? 0 : guide->sections.front();
        for (const size_t index : guide->sections) {
            if (m_blockY[index] <= scrollY + ui.spacing("48")) {
                current = index;
            }
        }
        const float itemH = ui.size("control");
        for (const size_t index : guide->sections) {
            const GuideBlock& section = guide->blocks[index];
            const float textH = ui.textSize(TypeRole::Label, section.text, kRailPx - 2.0F * ui.spacing("12")).y;
            const float h = std::max(itemH, textH + ui.spacing("12"));
            ImGui::SetCursorScreenPos(ImVec2(left, ry));
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::InvisibleButton("##section", ImVec2(kRailPx, h))) {
                m_scrollTo = m_blockY[index] - ui.spacing("24");
            }
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            const bool active = index == current;
            if (active || hovered) {
                outer.AddRectFilled(ImVec2(left, ry), ImVec2(left + kRailPx, ry + h),
                                    ui.color(active ? "accent/soft" : "bg/hover"), ui.radius("sm"));
            }
            ui.drawText(outer, TypeRole::Label,
                        ImVec2(left + ui.spacing("12"), ry + (h - textH) * 0.5F),
                        ui.color(active || hovered ? "text/primary" : "text/secondary"),
                        section.text, kRailPx - 2.0F * ui.spacing("12"));
            ry += h + ui.spacing("2");
        }
    }
    return actions;
}

}  // namespace zarescope::ui
