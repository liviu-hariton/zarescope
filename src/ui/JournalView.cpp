// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/JournalView.h"

#include "ui/Clock.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace zarescope::ui {

namespace {

// Body text measure: about 65 characters of 13-px Inter.
constexpr float kMeasurePx = 440.0F;

constexpr std::array<store::EventKind, 4> kKinds{store::EventKind::Pass, store::EventKind::Images,
                                                 store::EventKind::Aircraft,
                                                 store::EventKind::Recording};

StringId kindLabel(store::EventKind kind) {
    switch (kind) {
    case store::EventKind::Pass: return StringId::KindPass;
    case store::EventKind::Images: return StringId::KindImages;
    case store::EventKind::Aircraft: return StringId::KindAircraft;
    case store::EventKind::Recording: return StringId::KindRecording;
    }
    return StringId::KindPass;
}

// Each kind's colour, always next to its name: violet for passes, success
// for images, live for aircraft, recording for recordings.
std::string_view kindToken(store::EventKind kind) {
    switch (kind) {
    case store::EventKind::Pass: return "pass-imminent";
    case store::EventKind::Images: return "success";
    case store::EventKind::Aircraft: return "live";
    case store::EventKind::Recording: return "recording";
    }
    return "text/secondary";
}

}  // namespace

store::JournalFilter JournalView::filter() const {
    store::JournalFilter filter;
    if (m_kind > 0) {
        filter.kind = kKinds[static_cast<size_t>(m_kind - 1)];
    }
    filter.text = m_search.data();
    return filter;
}

JournalViewActions JournalView::draw(UiKit& ui, ImVec2 pos, ImVec2 size,
                                     const store::JournalService::Snapshot& snapshot) {
    JournalViewActions actions;
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float gap = ui.spacing("8");
    const float controlH = ui.size("control");

    // Filter bar.
    const std::array<std::string_view, 5> kinds{
        ui.tr(StringId::JournalAll), ui.tr(StringId::JournalPasses), ui.tr(StringId::JournalImages),
        ui.tr(StringId::JournalAircraft), ui.tr(StringId::JournalRecordings)};
    float x = pos.x;
    if (ui.segmented("journalKind", ImVec2(x, pos.y), kinds.data(), 5, m_kind)) {
        actions.filter = filter();
    }
    x += ui.segmentedWidth(kinds.data(), 5) + ui.spacing("16");

    const std::string_view json = ui.tr(StringId::ExportJson);
    const std::string_view csv = ui.tr(StringId::ExportCsv);
    float right = pos.x + size.x - ui.buttonWidth(json);
    if (ui.button("##exportJson", ImVec2(right, pos.y), json, ButtonKind::Secondary)) {
        actions.exportJson = true;
    }
    UiKit::tooltip(ui.tr(StringId::TipExport));
    right -= gap + ui.buttonWidth(csv);
    if (ui.button("##exportCsv", ImVec2(right, pos.y), csv, ButtonKind::Secondary)) {
        actions.exportJson = false;
    }
    UiKit::tooltip(ui.tr(StringId::TipExport));

    // The search field takes what's between, up to a comfortable width.
    const float searchW = std::min(right - ui.spacing("16") - x, ui.size("sidePanel"));
    if (searchW > controlH * 2.0F) {
        ImGui::SetCursorScreenPos(ImVec2(x, pos.y));
        ImGui::SetNextItemWidth(searchW);
        // Centre ImGui's frame in the control height.
        const float frameH = ImGui::GetFrameHeight();
        ImGui::SetCursorScreenPos(ImVec2(x, pos.y + (controlH - frameH) * 0.5F));
        if (ImGui::InputTextWithHint("##journalSearch",
                                     std::string(ui.tr(StringId::JournalSearch)).c_str(),
                                     m_search.data(), m_search.size())) {
            actions.filter = filter();
        }
    }

    float y = pos.y + controlH + gap;
    // The last export, or why the journal is unavailable.
    std::string note;
    std::string_view noteToken = "text/secondary";
    if (!snapshot.open && !snapshot.error.empty()) {
        note = ui.trf(StringId::JournalUnavailable, snapshot.error);
        noteToken = "warning";
    } else if (!snapshot.exportMessage.empty()) {
        note = ui.trf(StringId::JournalExported, snapshot.exportMessage);
    }
    if (!note.empty()) {
        ui.drawText(drawList, TypeRole::Caption, ImVec2(pos.x, y), ui.color(noteToken), note,
                    size.x);
        y += ui.textSize(TypeRole::Caption, note, size.x).y + gap;
    }

    if (snapshot.events.empty()) {
        const bool filtered = m_kind != 0 || m_search[0] != '\0';
        const float measure = std::min(kMeasurePx, size.x);
        ui.drawText(drawList, TypeRole::Body,
                    ImVec2(pos.x + (size.x - measure) * 0.5F, pos.y + size.y * 0.3F),
                    ui.color("text/secondary"),
                    ui.tr(filtered ? StringId::JournalNoMatch : StringId::JournalEmpty), measure);
        return actions;
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, y));
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_BordersInnerH |
                                       ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("##journal", 5, kFlags, ImVec2(size.x, pos.y + size.y - y))) {
        return actions;
    }
    const float rowHeight = ui.size("tableRowCompact");
    ImGui::TableSetupScrollFreeze(0, 1);
    const std::array<std::pair<StringId, float>, 4> columns{{
        {StringId::ColTime, 1.1F},
        {StringId::ColKind, 0.8F},
        {StringId::ColEvent, 1.4F},
        {StringId::ColDetail, 3.0F},
    }};
    for (const auto& [id, weight] : columns) {
        ImGui::TableSetupColumn(std::string(ui.tr(id)).c_str(), ImGuiTableColumnFlags_WidthStretch,
                                weight);
    }
    const std::string show(ui.tr(StringId::Show));
    ImGui::TableSetupColumn("##show", ImGuiTableColumnFlags_WidthFixed,
                            ui.buttonWidth(show) + gap);
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers, rowHeight);
    for (int column = 0; column < 4; ++column) {
        ImGui::TableSetColumnIndex(column);
        ImGui::PushFont(ui.face(TypeRole::Caption).font, ui.face(TypeRole::Caption).sizePx);
        ImGui::PushStyleColor(ImGuiCol_Text, ui.colorVec("text/secondary"));
        std::string upper = ImGui::TableGetColumnName(column);
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        ImGui::TextUnformatted(upper.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    const auto& mono = ui.face(TypeRole::MonoData);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(snapshot.events.size()), rowHeight);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const store::JournalEvent& event = snapshot.events[static_cast<size_t>(row)];
            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
            ImGui::PushID(static_cast<int>(event.id));
            const float textOffset = (rowHeight - ImGui::GetTextLineHeight()) * 0.5F;

            ImGui::TableSetColumnIndex(0);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
            ImGui::PushFont(mono.font, mono.sizePx);
            ImGui::TextUnformatted(formatLocalDateTime(event.timeUtc).c_str());
            ImGui::PopFont();

            ImGui::TableSetColumnIndex(1);
            {
                // A dot in the kind's colour, then its name.
                const ImVec2 at = ImGui::GetCursorScreenPos();
                const float dot = ui.spacing("8");
                drawList.AddCircleFilled(ImVec2(at.x + dot * 0.5F, at.y + rowHeight * 0.5F),
                                         dot * 0.5F, ui.color(kindToken(event.kind)));
                ImGui::SetCursorScreenPos(ImVec2(at.x + dot + gap, at.y + textOffset));
                ImGui::TextUnformatted(std::string(ui.tr(kindLabel(event.kind))).c_str());
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
            ImGui::TextUnformatted(event.title.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
            ImGui::PushStyleColor(ImGuiCol_Text, ui.colorVec("text/secondary"));
            ImGui::TextUnformatted(event.detail.c_str());
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(4);
            if (!event.path.empty()) {
                const ImVec2 at = ImGui::GetCursorScreenPos();
                if (ui.button("##show", ImVec2(at.x, at.y + (rowHeight - controlH) * 0.5F), show,
                              ButtonKind::Secondary)) {
                    actions.reveal = std::filesystem::path(event.path);
                }
                UiKit::tooltip(ui.tr(StringId::TipShow));
            }
            ImGui::PopID();
        }
    }
    ImGui::EndTable();
    return actions;
}

}  // namespace zarescope::ui
