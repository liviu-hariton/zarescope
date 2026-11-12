// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/SatelliteView.h"

#include "ui/Clock.h"
#include "ui/Format.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace zarescope::ui {

namespace {

using sat::Clock;
using namespace std::chrono;

// The progress bar fills over the last 15 minutes before AOS.
constexpr auto kCardLead = minutes(15);
// DESIGN.md §5: the next-pass progress bar is 6 px tall.
constexpr float kProgressHeight = 6.0F;
// Body text measure: about 65 characters of 15-px Inter.
constexpr float kMeasurePx = 520.0F;
// DESIGN.md §5: icons beside text are 16 px.
constexpr float kIconPx = 16.0F;

// "05:09" local time.
std::string localTime(Clock::time_point when) {
    return formatClock(when, ClockZone::Local).substr(0, 5);
}

const sat::Satellite* findSatellite(const std::vector<sat::Satellite>& catalog, uint32_t norad) {
    const auto it = std::find_if(catalog.begin(), catalog.end(),
                                 [norad](const sat::Satellite& s) { return s.noradId == norad; });
    return it == catalog.end() ? nullptr : &*it;
}

// Elevation → distance from the plot's centre: 90° at the centre, 0° at the rim.
float polarRadius(double elevationDeg, float radius) {
    return radius * static_cast<float>((90.0 - std::clamp(elevationDeg, 0.0, 90.0)) / 90.0);
}

ImVec2 polarPoint(const sat::LookAngle& look, ImVec2 centre, float radius) {
    const float r = polarRadius(look.elevationDeg, radius);
    const double a = look.azimuthDeg * std::numbers::pi / 180.0;
    return {centre.x + r * static_cast<float>(std::sin(a)),
            centre.y - r * static_cast<float>(std::cos(a))};
}

}  // namespace

std::string formatCountdown(seconds remaining) {
    const bool past = remaining.count() < 0;
    const int64_t total = std::abs(remaining.count());
    const int64_t h = total / 3600;
    const int64_t m = (total / 60) % 60;
    const int64_t s = total % 60;
    const std::string sign = past ? "+" : std::string(kMinus);
    if (h > 0) {
        return std::format("T{}{}:{:02}:{:02}", sign, h, m, s);
    }
    return std::format("T{}{:02}:{:02}", sign, m, s);
}

SatelliteViewActions SatelliteView::draw(UiKit& ui, ImVec2 pos, ImVec2 size,
                                         const sat::SatelliteSnapshot& snapshot,
                                         const std::vector<sat::Satellite>& catalog,
                                         const app::AutomationStatus* automation) {
    SatelliteViewActions actions;
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float measure = std::min(kMeasurePx, size.x);
    auto message = [&](std::string_view text) {
        ui.drawText(drawList, TypeRole::Body,
                    ImVec2(pos.x + (size.x - measure) * 0.5F, pos.y + size.y * 0.35F),
                    ui.color("text/secondary"), text, measure);
    };
    if (!snapshot.station) {
        message(ui.tr(StringId::NeedStationSatellites));
        return actions;
    }
    if (snapshot.tles.empty()) {
        message(snapshot.lastError.empty() ? std::string(ui.tr(StringId::WaitingForTle))
                                           : ui.trf(StringId::TleError, snapshot.lastError));
        return actions;
    }

    // Keep the selection on a pass that still exists; default to the next.
    const auto now = Clock::now();
    const auto upcoming =
        std::find_if(snapshot.passes.begin(), snapshot.passes.end(),
                     [now](const sat::PlannedPass& p) { return p.pass.los > now; });
    const bool selectionExists =
        m_selected &&
        std::any_of(snapshot.passes.begin(), snapshot.passes.end(), [&](const sat::PlannedPass& p) {
            return p.noradId == m_selected->first && p.pass.aos == m_selected->second;
        });
    if (!selectionExists) {
        m_selected.reset();
        if (upcoming != snapshot.passes.end()) {
            m_selected = std::make_pair(upcoming->noradId, upcoming->pass.aos);
        }
    }

    // Left column: the next-pass card over the polar plot. Right: the list.
    const float gap = ui.spacing("16");
    // DESIGN.md §6: the side panel (a size token).
    const float left = std::min(ui.size("sidePanel"), size.x * 0.4F);
    float y = pos.y;
    if (upcoming != snapshot.passes.end()) {
        // The card's pass is the one automation will take (or is taking).
        const auto& handled = automation == nullptr ? std::optional<sat::PlannedPass>{}
                              : automation->current ? std::optional(automation->current->pass)
                                                    : automation->next;
        const bool willRecord = automation != nullptr && automation->enabled && handled &&
                                handled->noradId == upcoming->noradId &&
                                handled->pass.aos == upcoming->pass.aos;
        drawCard(ui, ImVec2(pos.x, y), left, *upcoming, catalog, willRecord);
        y += m_cardHeight + gap;
    }
    drawPolar(ui, ImVec2(pos.x, y), ImVec2(left, pos.y + size.y - y), snapshot);

    // Right column: the automation panel over the pass list.
    const float listX = pos.x + left + gap;
    const float listWidth = size.x - left - gap;
    float listTop = pos.y;
    if (automation != nullptr) {
        listTop += drawAutomation(ui, ImVec2(listX, listTop), listWidth, *automation, actions) + gap;
    }
    drawPassList(ui, ImVec2(listX, listTop), ImVec2(listWidth, pos.y + size.y - listTop),
                 snapshot);
    return actions;
}

float SatelliteView::drawAutomation(UiKit& ui, ImVec2 pos, float width,
                                    const app::AutomationStatus& status,
                                    SatelliteViewActions& actions) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float pad = ui.spacing("16");
    const float gap = ui.spacing("8");
    const float inner = width - 2.0F * pad;
    const bool missing = status.satdump.probed && !status.satdump.executable;

    // Words first, measured, so the panel's background fits them.
    std::string satdumpLine;
    if (!status.satdump.probed) {
        satdumpLine = ui.tr(StringId::SatDumpLooking);
    } else if (status.satdump.executable) {
        satdumpLine = status.satdump.version.empty()
                          ? std::string(ui.tr(StringId::SatDumpReady))
                          : ui.trf(StringId::SatDumpReadyVersion, status.satdump.version);
    }
    std::string help(ui.tr(StringId::AutoRecordHelp));
    if (status.enabled) {
        help += " ";
        help += ui.tr(StringId::AutoRecordAwake);
    }
    const float rowH = ui.size("control");
    const float helpH = ui.textSize(TypeRole::Body, help, inner).y;
    const float iconGap = kIconPx + gap;
    const std::string_view missingText = ui.tr(StringId::SatDumpMissing);
    const std::string_view command = ui.tr(StringId::SatDumpInstallCommand);
    const float missingH = missing ? ui.textSize(TypeRole::Body, missingText, inner - iconGap).y +
                                         gap + ui.textSize(TypeRole::MonoData, command).y + gap +
                                         rowH
                                   : 0.0F;
    const float height = pad + rowH + gap + helpH + (missing ? ui.spacing("12") + missingH : 0.0F) +
                         pad;
    const ImVec2 max(pos.x + width, pos.y + height);
    drawList.AddRectFilled(pos, max, ui.color("bg/surface"), ui.radius("lg"));
    drawList.AddRect(pos, max, ui.color("border/subtle"), ui.radius("lg"), ui.border());

    const float x = pos.x + pad;
    float y = pos.y + pad;
    bool on = status.enabled;
    if (ui.toggle("##autoRecord", ImVec2(x, y), on, ui.tr(StringId::AutoRecordLabel))) {
        actions.autoRecord = on;
    }
    UiKit::tooltip(ui.tr(StringId::TipAutoRecord));
    // SatDump's state, right-aligned on the switch's row, when there's room.
    if (!satdumpLine.empty()) {
        const float textW = ui.textSize(TypeRole::Caption, satdumpLine).x;
        const float lineX = pos.x + width - pad - textW;
        const bool fits = lineX > x + ui.toggleWidth(ui.tr(StringId::AutoRecordLabel)) + pad;
        if (fits) {
            const bool ready = status.satdump.executable.has_value();
            if (ready) {
                ui.icon(drawList, Icon::Check,
                        ImVec2(lineX - gap - kIconPx * 0.5F, y + rowH * 0.5F), kIconPx,
                        ui.color("success"));
            }
            ui.drawTextCentered(drawList, TypeRole::Caption, lineX, y, rowH,
                                ui.color("text/secondary"), satdumpLine);
        }
    }
    y += rowH + gap;
    ui.drawText(drawList, TypeRole::Body, ImVec2(x, y), ui.color("text/secondary"), help, inner);
    y += helpH;

    if (missing) {
        // Warning colour plus the alert icon: never colour alone.
        y += ui.spacing("12");
        const float textH = ui.textSize(TypeRole::Body, missingText, inner - iconGap).y;
        ui.icon(drawList, Icon::Alert,
                ImVec2(x + kIconPx * 0.5F, y + ui.textSize(TypeRole::Body, "N").y * 0.5F), kIconPx,
                ui.color("warning"));
        ui.drawText(drawList, TypeRole::Body, ImVec2(x + iconGap, y), ui.color("text/primary"),
                    missingText, inner - iconGap);
        y += textH + gap;
        ui.drawText(drawList, TypeRole::MonoData, ImVec2(x + iconGap, y), ui.color("accent"),
                    command);
        y += ui.textSize(TypeRole::MonoData, command).y + gap;
        if (ui.button("##checkSatDump", ImVec2(x + iconGap, y), ui.tr(StringId::CheckAgain),
                      ButtonKind::Secondary)) {
            actions.checkSatDump = true;
        }
    }
    return height;
}

void SatelliteView::drawCard(UiKit& ui, ImVec2 pos, float width, const sat::PlannedPass& next,
                             const std::vector<sat::Satellite>& catalog, bool willRecord) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float pad = ui.spacing("16");
    const float gap = ui.spacing("8");
    const ImU32 violet = ui.color("pass-imminent");
    const auto now = Clock::now();

    // Measure the content first, so the card's background fits it.
    const float captionH = ui.textSize(TypeRole::Caption, "N").y;
    const float headingH = ui.textSize(TypeRole::Heading, "N").y;
    const float dataH = ui.textSize(TypeRole::MonoData, "0").y;
    const float countdownH = ui.textSize(TypeRole::MonoFreqCompact, "0").y;
    const float tileH = ui.textSize(TypeRole::Caption, "N").y + ui.spacing("4") + dataH;
    m_cardHeight = pad + captionH + gap + headingH + ui.spacing("4") + dataH + gap + countdownH + gap +
                   kProgressHeight + ui.spacing("12") + tileH + pad;
    const ImVec2 max(pos.x + width, pos.y + m_cardHeight);
    drawList.AddRectFilled(pos, max, ui.color("bg/surface"), ui.radius("lg"));
    drawList.AddRect(pos, max, ui.color("border/subtle"), ui.radius("lg"), ui.border());

    float x = pos.x + pad;
    float y = pos.y + pad;
    ui.drawText(drawList, TypeRole::Caption, ImVec2(x, y), violet, ui.tr(StringId::NextPass));
    if (willRecord) {
        // Right-aligned on the caption line: a recording dot with its label.
        const std::string_view label = ui.tr(StringId::WillRecord);
        const float labelX = pos.x + width - pad - ui.textSize(TypeRole::Caption, label).x;
        const float dot = ui.spacing("8");
        drawList.AddCircleFilled(ImVec2(labelX - gap - dot * 0.5F, y + captionH * 0.5F), dot * 0.5F,
                                 ui.color("recording"));
        ui.drawText(drawList, TypeRole::Caption, ImVec2(labelX, y), ui.color("text/secondary"),
                    label);
    }
    y += captionH + gap;
    ui.drawText(drawList, TypeRole::Heading, ImVec2(x, y), ui.color("text/primary"),
                next.satellite);
    y += headingH + ui.spacing("4");
    if (const sat::Satellite* satellite = findSatellite(catalog, next.noradId);
        satellite != nullptr && !satellite->downlinks.empty()) {
        const auto& link = satellite->downlinks.front();
        std::string mode = link.mode;
        std::transform(mode.begin(), mode.end(), mode.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        ui.drawText(drawList, TypeRole::MonoData, ImVec2(x, y), ui.color("text/secondary"),
                    formatFrequency(link.frequencyHz) + " · " + mode);
    }
    y += dataH + gap;

    // Countdown to AOS, or to LOS while the pass is on.
    const bool inProgress = next.pass.aos <= now;
    const auto target = inProgress ? next.pass.los : next.pass.aos;
    const auto remaining = duration_cast<seconds>(target - now);
    ui.drawText(drawList, TypeRole::MonoFreqCompact, ImVec2(x, y), violet,
                inProgress ? ui.trf(StringId::PassInProgress, formatDuration(remaining.count()))
                           : formatCountdown(remaining));
    y += countdownH + gap;

    // Progress over the last 15 minutes before AOS (full while it's on).
    const float barWidth = width - 2.0F * pad;
    const double fraction = inProgress
                                ? 1.0
                                : std::clamp(1.0 - duration<double>(next.pass.aos - now).count() /
                                                       duration<double>(kCardLead).count(),
                                             0.0, 1.0);
    drawList.AddRectFilled(ImVec2(x, y), ImVec2(x + barWidth, y + kProgressHeight),
                           ui.color("bg/hover"), ui.radius("xs"));
    drawList.AddRectFilled(ImVec2(x, y),
                           ImVec2(x + barWidth * static_cast<float>(fraction), y + kProgressHeight),
                           violet, ui.radius("xs"));
    y += kProgressHeight + ui.spacing("12");

    // Tiles: max elevation, AOS (time · azimuth), LOS (time · azimuth).
    const std::array<std::pair<std::string_view, std::string>, 3> tiles{{
        {ui.tr(StringId::MaxElevationShort), std::format("{:.0f}°", next.pass.maxElevationDeg)},
        {"AOS", localTime(next.pass.aos) + " · " + std::format("{:.0f}°", next.pass.aosAzimuthDeg)},
        {"LOS", localTime(next.pass.los) + " · " + std::format("{:.0f}°", next.pass.losAzimuthDeg)},
    }};
    const float tileWidth = barWidth / 3.0F;
    for (size_t i = 0; i < tiles.size(); ++i) {
        const float tx = x + tileWidth * static_cast<float>(i);
        ui.drawText(drawList, TypeRole::Caption, ImVec2(tx, y), ui.color("text/secondary"),
                    tiles[i].first);
        ui.drawText(drawList, TypeRole::MonoData, ImVec2(tx, y + captionH + ui.spacing("4")),
                    ui.color("text/primary"), tiles[i].second);
    }
}

void SatelliteView::drawPassList(UiKit& ui, ImVec2 pos, ImVec2 size,
                                 const sat::SatelliteSnapshot& snapshot) {
    const auto now = Clock::now();
    const float statusHeight = ui.textSize(TypeRole::MonoSmall, "0").y + ui.spacing("8");

    ImGui::SetCursorScreenPos(pos);
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_BordersInnerH |
                                       ImGuiTableFlags_SizingStretchProp;
    if (snapshot.passes.empty()) {
        ui.drawText(*ImGui::GetWindowDrawList(), TypeRole::Body,
                    ImVec2(pos.x, pos.y + ui.spacing("16")), ui.color("text/secondary"),
                    ui.trf(StringId::NoPasses, std::format("{:.0f}", snapshot.minElevationDeg)),
                    std::min(size.x, kMeasurePx));
    } else if (ImGui::BeginTable("##passes", 5, kFlags, ImVec2(size.x, size.y - statusHeight))) {
        const float rowHeight = ui.size("tableRowCompact");
        ImGui::TableSetupScrollFreeze(0, 1);
        const std::array<std::pair<StringId, float>, 5> columns{{
            {StringId::ColSatellite, 1.6F},
            {StringId::ColStart, 1.4F},
            {StringId::MaxElevationShort, 0.8F},
            {StringId::ColDuration, 0.8F},
            {StringId::ColDirection, 1.0F},
        }};
        for (const auto& [id, weight] : columns) {
            ImGui::TableSetupColumn(std::string(ui.tr(id)).c_str(),
                                    ImGuiTableColumnFlags_WidthStretch, weight);
        }
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers, rowHeight);
        for (int column = 0; column < 5; ++column) {
            ImGui::TableSetColumnIndex(column);
            ImGui::PushFont(ui.face(TypeRole::Caption).font, ui.face(TypeRole::Caption).sizePx);
            ImGui::PushStyleColor(ImGuiCol_Text, ui.colorVec("text/secondary"));
            std::string upper = ImGui::TableGetColumnName(column);
            std::transform(upper.begin(), upper.end(), upper.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            UiKit::centreInRow(rowHeight);
            ImGui::TextUnformatted(upper.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
        const auto& mono = ui.face(TypeRole::MonoData);
        for (const sat::PlannedPass& planned : snapshot.passes) {
            const sat::Pass& pass = planned.pass;
            const bool past = pass.los < now;
            const bool selected = m_selected && m_selected->first == planned.noradId &&
                                  m_selected->second == pass.aos;
            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
            if (selected) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ui.color("accent/soft"));
            }
            ImGui::PushID(static_cast<int>(planned.noradId));
            ImGui::PushID(
                static_cast<int>(duration_cast<seconds>(pass.aos.time_since_epoch()).count()));
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ui.colorVec(past ? "text/disabled" : "text/primary"));
            if (UiKit::rowSelectable(planned.satellite.c_str(), selected, rowHeight)) {
                m_selected = std::make_pair(planned.noradId, pass.aos);
            }
            ImGui::PopStyleColor();
            ImGui::PushFont(mono.font, mono.sizePx);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ui.colorVec(past ? "text/disabled" : "text/primary"));
            ImGui::TableSetColumnIndex(1);
            UiKit::centreInRow(rowHeight);
            // Local time, with the day when it isn't today.
            const auto aosDay = floor<days>(pass.aos);
            const auto today = floor<days>(now);
            std::string start = localTime(pass.aos);
            if (aosDay > today) {
                start += std::format(" (+{})", (aosDay - today).count());
            }
            ImGui::TextUnformatted(start.c_str());
            ImGui::TableSetColumnIndex(2);
            UiKit::centreInRow(rowHeight);
            ImGui::TextUnformatted(std::format("{:.0f}°", pass.maxElevationDeg).c_str());
            ImGui::TableSetColumnIndex(3);
            UiKit::centreInRow(rowHeight);
            ImGui::TextUnformatted(
                formatDuration(duration_cast<seconds>(pass.los - pass.aos).count()).c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::TableSetColumnIndex(4);
            UiKit::centreInRow(rowHeight);
            ImGui::TextUnformatted(
                std::string(ui.tr(pass.northbound ? StringId::Northbound : StringId::Southbound))
                    .c_str());
            ImGui::PopID();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // Where the orbits come from, and how old they are.
    Clock::time_point oldest = now;
    for (const auto& [norad, info] : snapshot.tles) {
        oldest = std::min(oldest, info.fetched);
    }
    const auto ageHours = duration_cast<hours>(now - oldest).count();
    std::string status = ui.trf(
        StringId::TleAge, ageHours < 1 ? std::string("< 1 h") : std::format("{} h", ageHours));
    if (!snapshot.lastError.empty()) {
        status += " · " + ui.trf(StringId::TleError, snapshot.lastError);
    }
    ui.drawText(*ImGui::GetWindowDrawList(), TypeRole::MonoSmall,
                ImVec2(pos.x, pos.y + size.y - statusHeight + ui.spacing("4")),
                ui.color(snapshot.lastError.empty() ? "text/secondary" : "warning"), status);
}

void SatelliteView::drawPolar(UiKit& ui, ImVec2 pos, ImVec2 size,
                              const sat::SatelliteSnapshot& snapshot) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    if (size.y < 80.0F) {
        return;
    }
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    drawList.AddRectFilled(pos, max, ui.color("bg/surface"), ui.radius("lg"));
    drawList.AddRect(pos, max, ui.color("border/subtle"), ui.radius("lg"), ui.border());

    const ImVec2 centre(pos.x + size.x * 0.5F, pos.y + size.y * 0.5F);
    const float radius = std::min(size.x, size.y) * 0.5F - ui.spacing("24");
    const ImU32 ring = ui.color("border/subtle");
    const ImU32 label = ui.color("text/secondary");
    // Rings at 0°, 30° and 60° elevation.
    for (double elevation : {0.0, 30.0, 60.0}) {
        drawList.AddCircle(centre, polarRadius(elevation, radius), ring, 0, ui.border());
        if (elevation > 0.0) {
            ui.drawText(drawList, TypeRole::MonoSmall,
                        ImVec2(centre.x + 3.0F, centre.y - polarRadius(elevation, radius) + 2.0F),
                        label, std::format("{:.0f}°", elevation));
        }
    }
    const std::array<std::pair<StringId, ImVec2>, 4> compass{{
        {StringId::CompassNorth, ImVec2(centre.x, centre.y - radius - 12.0F)},
        {StringId::CompassEast, ImVec2(centre.x + radius + 10.0F, centre.y)},
        {StringId::CompassSouth, ImVec2(centre.x, centre.y + radius + 12.0F)},
        {StringId::CompassWest, ImVec2(centre.x - radius - 10.0F, centre.y)},
    }};
    for (const auto& [id, where] : compass) {
        const std::string_view text = ui.tr(id);
        const ImVec2 textSize = ui.textSize(TypeRole::Label, text);
        ui.drawText(drawList, TypeRole::Label,
                    ImVec2(where.x - textSize.x * 0.5F, where.y - textSize.y * 0.5F), label, text);
    }

    // The selected pass's track, computed once per selection.
    if (!m_selected || !snapshot.station) {
        return;
    }
    const auto planned = std::find_if(
        snapshot.passes.begin(), snapshot.passes.end(), [&](const sat::PlannedPass& p) {
            return p.noradId == m_selected->first && p.pass.aos == m_selected->second;
        });
    const auto tle = snapshot.tles.find(m_selected->first);
    if (planned == snapshot.passes.end() || tle == snapshot.tles.end()) {
        return;
    }
    try {
        if (!m_propagator || m_propagatorLine1 != tle->second.tle.line1) {
            m_propagator = std::make_unique<sat::Propagator>(tle->second.tle);
            m_propagatorLine1 = tle->second.tle.line1;
            m_trackFor.reset();
        }
        if (m_trackFor != m_selected) {
            m_track = sat::passTrack(*m_propagator, *snapshot.station, planned->pass, 90);
            m_trackFor = m_selected;
        }
    } catch (const std::exception&) {
        return;  // a TLE SGP4 can't use: no track
    }
    if (m_track.size() < 2) {
        return;
    }

    // Solid where flown, dotted where still to come (DESIGN.md §5).
    const auto now = Clock::now();
    const sat::Pass& pass = planned->pass;
    const double progress = std::clamp(
        duration<double>(now - pass.aos).count() / duration<double>(pass.los - pass.aos).count(),
        0.0, 1.0);
    const auto flownPoints =
        static_cast<size_t>(std::lround(progress * static_cast<double>(m_track.size() - 1)));
    const ImU32 accent = ui.color("accent");
    const float stroke = ui.tokens().border("focus").value_or(2.0F);
    for (size_t i = 1; i < m_track.size(); ++i) {
        const ImVec2 a = polarPoint(m_track[i - 1], centre, radius);
        const ImVec2 b = polarPoint(m_track[i], centre, radius);
        if (i <= flownPoints) {
            drawList.AddLine(a, b, accent, stroke);
        } else if (i % 2 == 0) {
            drawList.AddCircleFilled(b, stroke * 0.75F, accent);  // dotted
        }
    }
    // AOS marker, the culmination label, and the satellite while it's up.
    drawList.AddCircle(polarPoint(m_track.front(), centre, radius), 4.0F, accent, 0, ui.border());
    const auto highest = std::max_element(m_track.begin(), m_track.end(),
                                          [](const sat::LookAngle& a, const sat::LookAngle& b) {
                                              return a.elevationDeg < b.elevationDeg;
                                          });
    const ImVec2 top = polarPoint(*highest, centre, radius);
    ui.drawText(drawList, TypeRole::MonoSmall, ImVec2(top.x + 6.0F, top.y - 6.0F),
                ui.color("text/primary"), std::format("{:.0f}°", pass.maxElevationDeg));
    if (progress > 0.0 && progress < 1.0) {
        const ImVec2 satellite = polarPoint(m_track[flownPoints], centre, radius);
        drawList.AddCircleFilled(satellite, 7.0F,
                                 ui.color("bg/surface"));  // knockout, like the logo
        drawList.AddCircleFilled(satellite, 5.0F, accent);
    }
}

}  // namespace zarescope::ui
