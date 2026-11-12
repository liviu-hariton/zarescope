// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/AboutView.h"

#include "ui/LogoMark.h"

#include <algorithm>

namespace zarescope::ui {

namespace {

// DESIGN.md: the About window is 400 px wide, the logo 80 px.
constexpr float kWidthPx = 400.0F;
constexpr float kLogoPx = 80.0F;
// How long "Copied" replaces "Copy version info".
constexpr auto kCopiedFor = std::chrono::seconds(2);

}  // namespace

AboutView::Actions AboutView::draw(UiKit& ui, const AboutInfo& info) {
    Actions actions;
    if (m_openRequested) {
        ImGui::OpenPopup("##about");
        m_openRequested = false;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    const float pad = ui.spacing("32");
    ImGui::SetNextWindowSize(ImVec2(kWidthPx + 2.0F * pad, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui.radius("lg"));
    const bool open = ImGui::BeginPopupModal(
        "##about", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleVar(2);
    m_isOpen = open;
    if (!open) {
        m_closeRequested = false;
        return actions;
    }

    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float centre = origin.x + kWidthPx * 0.5F;
    float y = origin.y;
    // Centred text, each wrapped line centred on its own (a centred column
    // with ragged left edges reads better than a left-aligned block).
    auto centred = [&](TypeRole role, std::string_view text, std::string_view token) {
        const float lineH = ui.textSize(role, "N").y;
        std::string line;
        auto emit = [&] {
            const float w = ui.textSize(role, line).x;
            ui.drawText(drawList, role, ImVec2(centre - w * 0.5F, y), ui.color(token), line);
            y += lineH;
            line.clear();
        };
        size_t start = 0;
        while (start < text.size()) {
            size_t end = text.find(' ', start);
            if (end == std::string_view::npos) {
                end = text.size();
            }
            const std::string_view word = text.substr(start, end - start);
            const std::string candidate =
                line.empty() ? std::string(word) : line + " " + std::string(word);
            if (!line.empty() && ui.textSize(role, candidate).x > kWidthPx) {
                emit();
                line = std::string(word);
            } else {
                line = candidate;
            }
            start = end + 1;
        }
        if (!line.empty()) {
            emit();
        }
    };

    drawLogoSymbol(drawList, ImVec2(centre - kLogoPx * 0.5F, y), kLogoPx,
                   {ui.logoColor("rings"), ui.logoColor("pass"), ui.color("bg/raised")});
    y += kLogoPx + ui.spacing("16");
    centred(TypeRole::Title, "Zarescope", "text/primary");
    y += ui.spacing("4");
    centred(TypeRole::Body, ui.trf(StringId::AboutVersion, info.version), "text/secondary");
    centred(TypeRole::MonoSmall, info.build, "text/secondary");
    y += ui.spacing("16");
    centred(TypeRole::Body, ui.tr(StringId::AboutTagline), "text/primary");
    y += ui.spacing("12");
    {
        // The receive-only note: a check in the success colour, then the words.
        const std::string_view note = ui.tr(StringId::AboutReceiveOnly);
        const float iconPx = ui.textSize(TypeRole::Body, "N").y;
        const float noteW = iconPx + ui.spacing("8") + ui.textSize(TypeRole::Body, note).x;
        const float x = centre - std::min(noteW, kWidthPx) * 0.5F;
        ui.icon(drawList, Icon::Check, ImVec2(x + iconPx * 0.5F, y + iconPx * 0.5F), iconPx,
                ui.color("success"));
        ui.drawText(drawList, TypeRole::Body, ImVec2(x + iconPx + ui.spacing("8"), y),
                    ui.color("text/primary"), note, kWidthPx - iconPx - ui.spacing("8"));
        y += ui.textSize(TypeRole::Body, note, kWidthPx - iconPx - ui.spacing("8")).y;
    }
    y += ui.spacing("24");

    // Project page and Copy version info, side by side, centred.
    const bool copied = std::chrono::steady_clock::now() - m_copiedAt < kCopiedFor;
    const std::string_view project = ui.tr(StringId::AboutProjectPage);
    const std::string_view copy =
        copied ? ui.tr(StringId::AboutCopied) : ui.tr(StringId::AboutCopyInfo);
    const float buttonsW = ui.buttonWidth(project) + ui.spacing("8") +
                           ui.buttonWidth(ui.tr(StringId::AboutCopyInfo));
    float bx = centre - buttonsW * 0.5F;
    if (ui.button("##aboutProject", ImVec2(bx, y), project, ButtonKind::Secondary)) {
        actions.openWebsite = true;
    }
    bx += ui.buttonWidth(project) + ui.spacing("8");
    if (ui.button("##aboutCopy", ImVec2(bx, y), copy, ButtonKind::Secondary,
                  copied ? Icon::Check : Icon::None)) {
        ImGui::SetClipboardText(("Zarescope " + info.version + " (" + info.build + ")").c_str());
        m_copiedAt = std::chrono::steady_clock::now();
    }
    y += ui.size("control") + ui.spacing("24");

    // Licence, credits and copyright: small, faint, under a hairline.
    drawList.AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + kWidthPx, y + ui.border()),
                           ui.color("border/subtle"));
    y += ui.spacing("16");
    centred(TypeRole::Caption, ui.tr(StringId::AboutLicence), "text/secondary");
    y += ui.spacing("8");
    centred(TypeRole::Caption, ui.tr(StringId::AboutCredits), "text/secondary");
    y += ui.spacing("8");
    centred(TypeRole::Caption, ui.trf(StringId::AboutCopyright, info.copyrightYears),
            "text/secondary");
    y += ui.spacing("24");

    const std::string_view closeLabel = ui.tr(StringId::Close);
    const float closeW = ui.buttonWidth(closeLabel);
    const bool closeClicked = ui.button("##aboutClose", ImVec2(centre - closeW * 0.5F, y),
                                        closeLabel, ButtonKind::Primary);
    y += ui.size("control");
    ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
    ImGui::Dummy(ImVec2(kWidthPx, 0.0F));

    if (closeClicked || m_closeRequested || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
        m_closeRequested = false;
        m_isOpen = false;
    }
    ImGui::EndPopup();
    return actions;
}

}  // namespace zarescope::ui
