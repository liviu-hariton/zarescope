// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/DiscoveryView.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

namespace zarescope::ui {

namespace {

// DESIGN.md §5: icons beside text are 16 px.
constexpr float kIconPx = 16.0F;

enum class EntryAction { Listen, Aircraft, Satellites };

struct Entry {
    const char* presetId;
    StringId title;
    StringId text;
    StringId antenna;
    EntryAction action;
};

struct Category {
    StringId name;
    std::array<Entry, 2> entries;
    int count;
};

constexpr std::array<Category, 4> kCategories{{
    {StringId::CatBroadcast,
     {{{"fm-broadcast", StringId::DiscFmTitle, StringId::DiscFmText, StringId::DiscFmAntenna,
        EntryAction::Listen}}},
     1},
    {StringId::CatAviation,
     {{{"airband", StringId::DiscAirbandTitle, StringId::DiscAirbandText,
        StringId::DiscAirbandAntenna, EntryAction::Listen},
       {"adsb", StringId::DiscAdsbTitle, StringId::DiscAdsbText, StringId::DiscAdsbAntenna,
        EntryAction::Aircraft}}},
     2},
    {StringId::CatAmateur,
     {{{"amateur-2m", StringId::Disc2mTitle, StringId::Disc2mText, StringId::Disc2mAntenna,
        EntryAction::Listen},
       {"pmr446", StringId::DiscPmrTitle, StringId::DiscPmrText, StringId::DiscPmrAntenna,
        EntryAction::Listen}}},
     2},
    {StringId::CatSatellites,
     {{{"meteor", StringId::DiscMeteorTitle, StringId::DiscMeteorText, StringId::DiscMeteorAntenna,
        EntryAction::Satellites}}},
     1},
}};

std::string upper(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

}  // namespace

DiscoveryActions DiscoveryView::draw(UiKit& ui, ImVec2 pos, ImVec2 size,
                                     const DiscoveryState& state) {
    DiscoveryActions actions;
    // A scrolling child: the page is taller than the window on a laptop.
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui.colorVec("bg/base"));
    ImGui::BeginChild("##discover", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor();
    ImDrawList& drawList = *ImGui::GetWindowDrawList();

    const float gap = ui.spacing("8");
    const float pad = ui.spacing("16");
    const float section = ui.spacing("32");
    // One readable column, centred: two side panels wide at most.
    const float width = std::min(size.x - 2.0F * pad, ui.size("sidePanel") * 2.0F);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float left = origin.x + std::max(pad, (size.x - width) * 0.5F);
    float y = origin.y + ui.spacing("24");

    // --- Start here -----------------------------------------------------------
    ui.drawText(drawList, TypeRole::Title, ImVec2(left, y), ui.color("text/primary"),
                ui.tr(StringId::DiscoverTitle));
    y += ui.textSize(TypeRole::Title, "N").y + gap;
    ui.drawText(drawList, TypeRole::Body, ImVec2(left, y), ui.color("text/secondary"),
                ui.tr(StringId::DiscoverIntro), width);
    y += ui.textSize(TypeRole::Body, ui.tr(StringId::DiscoverIntro), width).y + ui.spacing("16");

    struct Step {
        StringId text;
        bool done;
        std::optional<StringId> action;
    };
    const std::array<Step, 3> steps{{
        {StringId::StepLocation, state.hasStation, StringId::StepLocationAction},
        {StringId::StepReceiver, state.receiverRunning, StringId::StepReceiverAction},
        {StringId::StepPick, false, std::nullopt},
    }};
    bool primaryGiven = false;
    const float rowH = ui.size("hitTarget");
    const float badge = ui.size("controlCompact");
    for (size_t i = 0; i < steps.size(); ++i) {
        const Step& step = steps[i];
        // The step's number in a ring, or a check once it's done.
        const ImVec2 centre(left + badge * 0.5F, y + rowH * 0.5F);
        if (step.done) {
            drawList.AddCircleFilled(centre, badge * 0.5F, ui.color("success/soft"));
            ui.icon(drawList, Icon::Check, centre, kIconPx, ui.color("success"));
        } else {
            drawList.AddCircle(centre, badge * 0.5F - ui.border(), ui.color("border/strong"), 0,
                               ui.border());
            const std::string number = std::to_string(i + 1);
            const ImVec2 numberSize = ui.textSize(TypeRole::Label, number);
            ui.drawText(drawList, TypeRole::Label,
                        ImVec2(centre.x - numberSize.x * 0.5F, centre.y - numberSize.y * 0.5F),
                        ui.color("text/primary"), number);
        }
        float textRight = left + width;
        if (step.action && !step.done) {
            const std::string_view label = ui.tr(*step.action);
            const float buttonW = ui.buttonWidth(label);
            textRight -= buttonW + ui.spacing("16");
            const ButtonKind kind = primaryGiven ? ButtonKind::Secondary : ButtonKind::Primary;
            primaryGiven = true;
            const std::string id = "##step" + std::to_string(i);
            if (ui.button(id.c_str(),
                          ImVec2(left + width - buttonW, y + (rowH - ui.size("control")) * 0.5F),
                          label, kind)) {
                (i == 0 ? actions.openSettings : actions.openDevices) = true;
            }
        }
        const float textX = left + badge + ui.spacing("12");
        const std::string_view text = ui.tr(step.text);
        const float textW = textRight - textX;
        const float textH = ui.textSize(TypeRole::Body, text, textW).y;
        ui.drawText(drawList, TypeRole::Body, ImVec2(textX, y + std::max(0.0F, (rowH - textH) * 0.5F)),
                    ui.color(step.done ? "text/secondary" : "text/primary"), text, textW);
        y += std::max(rowH, textH) + gap;
    }

    // --- Categories -------------------------------------------------------------
    for (const Category& category : kCategories) {
        y += section - gap;
        ui.drawText(drawList, TypeRole::Caption, ImVec2(left, y), ui.color("text/secondary"),
                    upper(ui.tr(category.name)));
        y += ui.textSize(TypeRole::Caption, "N").y + gap;
        drawList.AddRectFilled(ImVec2(left, y), ImVec2(left + width, y + ui.border()),
                               ui.color("border/subtle"));
        y += ui.spacing("12");

        for (int e = 0; e < category.count; ++e) {
            const Entry& entry = category.entries[static_cast<size_t>(e)];
            // One action, worded for what it will do right now.
            StringId label = StringId::ActionListen;
            bool needsReceiver = false;
            switch (entry.action) {
            case EntryAction::Listen:
                needsReceiver = !state.receiverIsDevice;
                label = needsReceiver ? StringId::ActionConnectFirst : StringId::ActionListen;
                break;
            case EntryAction::Aircraft:
                needsReceiver = !state.receiverIsDevice;
                label = needsReceiver ? StringId::ActionConnectFirst : StringId::ActionShowAircraft;
                break;
            case EntryAction::Satellites: label = StringId::ActionPlanPasses; break;
            }
            const float buttonW = ui.buttonWidth(ui.tr(label));
            const float textW = width - buttonW - ui.spacing("24");

            const float top = y;
            ui.drawText(drawList, TypeRole::Heading, ImVec2(left, y), ui.color("text/primary"),
                        ui.tr(entry.title));
            const std::string id = std::string("##entry-") + entry.presetId;
            if (ui.button(id.c_str(), ImVec2(left + width - buttonW, top), ui.tr(label),
                          ButtonKind::Secondary)) {
                if (needsReceiver) {
                    actions.openDevices = true;
                } else if (entry.action == EntryAction::Satellites) {
                    actions.showSatellites = true;
                } else {
                    actions.applyPreset = entry.presetId;
                    actions.showAircraft = entry.action == EntryAction::Aircraft;
                }
            }
            y += ui.textSize(TypeRole::Heading, "N").y + ui.spacing("4");
            ui.drawText(drawList, TypeRole::Body, ImVec2(left, y), ui.color("text/primary"),
                        ui.tr(entry.text), textW);
            y += ui.textSize(TypeRole::Body, ui.tr(entry.text), textW).y + ui.spacing("4");
            ui.drawText(drawList, TypeRole::Body, ImVec2(left, y), ui.color("text/secondary"),
                        ui.tr(entry.antenna), textW);
            y += ui.textSize(TypeRole::Body, ui.tr(entry.antenna), textW).y;
            y = std::max(y, top + ui.size("control")) + ui.spacing("24");
        }
    }

    // Tell the child how tall the page is, so it scrolls.
    ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
    ImGui::Dummy(ImVec2(0.0F, pad));
    ImGui::EndChild();
    return actions;
}

}  // namespace zarescope::ui
