// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/AircraftView.h"

#include "ui/Format.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>
#include <vector>

namespace zarescope::ui {

namespace {

using adsb::Aircraft;
using Clock = adsb::Clock;

// DESIGN.md §5: rows not heard for 30 s are dimmed.
constexpr auto kStale = std::chrono::seconds(30);
// The table takes this share of the width, at most this many points.
constexpr float kTableShare = 0.45F;
constexpr float kTableMaxWidth = 600.0F;
// Radar ranges to choose from (km): the smallest that holds every aircraft.
constexpr std::array kRanges{25.0, 50.0, 100.0, 200.0, 300.0, 400.0};
// Clicks within this many points of an aircraft select it.
constexpr float kPickRadius = 14.0F;

std::string icaoText(uint32_t icao) { return std::format("{:06X}", icao); }

std::string nameOf(const Aircraft& aircraft) {
    return aircraft.callsign.empty() ? icaoText(aircraft.icao) : aircraft.callsign;
}

// A filled arrow pointing along `trackDeg` (0 = north, clockwise).
void arrow(ImDrawList& drawList, ImVec2 c, float size, double trackDeg, ImU32 color) {
    const double a = trackDeg * std::numbers::pi / 180.0;
    const auto s = static_cast<float>(std::sin(a));
    const auto co = static_cast<float>(std::cos(a));
    auto point = [&](float forward, float side) {
        // forward along the track, side to its right
        return ImVec2(c.x + forward * s + side * co, c.y - forward * co + side * s);
    };
    drawList.AddTriangleFilled(point(size, 0.0F), point(-size * 0.7F, size * 0.6F),
                               point(-size * 0.7F, -size * 0.6F), color);
}

}  // namespace

std::string formatThousands(int value) {
    const bool negative = value < 0;
    std::string digits = std::to_string(std::abs(value));
    std::string out;
    const size_t first = digits.size() % 3 == 0 ? 3 : digits.size() % 3;
    out += digits.substr(0, first);
    for (size_t i = first; i < digits.size(); i += 3) {
        out += kThinSpace;
        out += digits.substr(i, 3);
    }
    return negative ? std::string(kMinus) + out : out;
}

void AircraftView::draw(UiKit& ui, ImVec2 pos, ImVec2 size, const adsb::AircraftStore& store) {
    if (m_selected && !store.aircraft().contains(*m_selected)) {
        m_selected.reset();
    }
    const float tableWidth = std::min(kTableMaxWidth, size.x * kTableShare);
    drawTable(ui, pos, ImVec2(tableWidth, size.y), store);
    const float gap = ui.spacing("16");
    drawRadar(ui, ImVec2(pos.x + tableWidth + gap, pos.y),
              ImVec2(size.x - tableWidth - gap, size.y), store);
}

void AircraftView::drawTable(UiKit& ui, ImVec2 pos, ImVec2 size, const adsb::AircraftStore& store) {
    const auto now = Clock::now();
    // Closest first; aircraft without a position after them, by callsign.
    std::vector<const Aircraft*> rows;
    for (const auto& [icao, aircraft] : store.aircraft()) {
        rows.push_back(&aircraft);
    }
    const auto& station = store.station();
    auto distance = [&](const Aircraft* a) {
        return station && a->position ? adsb::distanceKm(*station, *a->position) : 1e9;
    };
    std::sort(rows.begin(), rows.end(), [&](const Aircraft* a, const Aircraft* b) {
        const double da = distance(a);
        const double db = distance(b);
        return da != db ? da < db : nameOf(*a) < nameOf(*b);
    });

    ImGui::SetCursorScreenPos(pos);
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_BordersInnerH |
                                       ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("##aircraft", 6, kFlags, size)) {
        return;
    }
    const float rowHeight = ui.size("tableRowCompact");
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("##heading", ImGuiTableColumnFlags_WidthFixed, 24.0F);
    ImGui::TableSetupColumn(std::string(ui.tr(StringId::ColCallsign)).c_str(),
                            ImGuiTableColumnFlags_WidthStretch, 1.4F);
    ImGui::TableSetupColumn(std::string(ui.tr(StringId::ColAltitude)).c_str(),
                            ImGuiTableColumnFlags_WidthStretch, 1.2F);
    ImGui::TableSetupColumn(std::string(ui.tr(StringId::ColSpeed)).c_str(),
                            ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn(std::string(ui.tr(StringId::ColDistance)).c_str(),
                            ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn(std::string(ui.tr(StringId::ColSeen)).c_str(),
                            ImGuiTableColumnFlags_WidthStretch, 0.7F);

    // Header in the caption style: small capitals, secondary colour.
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers, rowHeight);
    for (int column = 0; column < 6; ++column) {
        ImGui::TableSetColumnIndex(column);
        const char* name = ImGui::TableGetColumnName(column);
        if (name[0] != '#') {
            ImGui::PushFont(ui.face(TypeRole::Caption).font, ui.face(TypeRole::Caption).sizePx);
            ImGui::PushStyleColor(ImGuiCol_Text, ui.colorVec("text/secondary"));
            std::string upper = name;
            std::transform(upper.begin(), upper.end(), upper.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            // Numeric columns (altitude onwards) are right-aligned, like their values.
            if (column >= 2) {
                const float textWidth = ImGui::CalcTextSize(upper.c_str()).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                                     textWidth);
            }
            ImGui::TextUnformatted(upper.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
    }

    // A cell's number right-aligned in mono with its unit in secondary.
    auto number = [&](const std::string& value, std::string_view unit, bool dim) {
        const auto& mono = ui.face(TypeRole::MonoData);
        const float unitWidth =
            unit.empty() ? 0.0F : ui.textSize(TypeRole::MonoData, unit).x + 4.0F;
        const float width = ui.textSize(TypeRole::MonoData, value).x + unitWidth;
        const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        const ImVec2 at(right - width, ImGui::GetCursorScreenPos().y);
        ImDrawList& dl = *ImGui::GetWindowDrawList();
        dl.AddText(mono.font, mono.sizePx, at, ui.color(dim ? "text/disabled" : "text/primary"),
                   value.c_str());
        if (!unit.empty()) {
            dl.AddText(mono.font, mono.sizePx, ImVec2(right - unitWidth + 4.0F, at.y),
                       ui.color(dim ? "text/disabled" : "text/secondary"),
                       std::string(unit).c_str());
        }
    };

    for (const Aircraft* aircraft : rows) {
        const bool stale = now - aircraft->lastSeen > kStale;
        const bool selected = m_selected == aircraft->icao;
        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        if (selected) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ui.color("accent/soft"));
        }
        ImGui::PushID(static_cast<int>(aircraft->icao));
        // Heading arrow.
        ImGui::TableSetColumnIndex(0);
        const ImVec2 cell = ImGui::GetCursorScreenPos();
        if (aircraft->trackDeg) {
            arrow(*ImGui::GetWindowDrawList(),
                  ImVec2(cell.x + 10.0F, cell.y + ImGui::GetTextLineHeight() * 0.5F), 7.0F,
                  *aircraft->trackDeg, ui.color(stale ? "text/disabled" : "accent"));
        }
        // Callsign; the whole row selects.
        ImGui::TableSetColumnIndex(1);
        ImGui::PushStyleColor(ImGuiCol_Text, ui.colorVec(stale ? "text/disabled" : "text/primary"));
        if (ImGui::Selectable(nameOf(*aircraft).c_str(), selected,
                              ImGuiSelectableFlags_SpanAllColumns)) {
            m_selected = aircraft->icao;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ICAO %s · %llu", icaoText(aircraft->icao).c_str(),
                              static_cast<unsigned long long>(aircraft->messages));
        }
        ImGui::TableSetColumnIndex(2);
        if (aircraft->altitudeFt) {
            number(formatThousands(*aircraft->altitudeFt), "ft", stale);
        }
        ImGui::TableSetColumnIndex(3);
        if (aircraft->groundSpeedKt) {
            number(std::format("{:.0f}", *aircraft->groundSpeedKt), "kt", stale);
        }
        ImGui::TableSetColumnIndex(4);
        if (station && aircraft->position) {
            number(std::format("{:.1f}", adsb::distanceKm(*station, *aircraft->position)), "km",
                   stale);
        }
        ImGui::TableSetColumnIndex(5);
        const auto seen =
            std::chrono::duration_cast<std::chrono::seconds>(now - aircraft->lastSeen).count();
        number(std::to_string(std::max<int64_t>(seen, 0)), "s", stale);
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void AircraftView::drawRadar(UiKit& ui, ImVec2 pos, ImVec2 size, const adsb::AircraftStore& store) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const auto& station = store.station();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    drawList.AddRectFilled(pos, max, ui.color("bg/surface"), ui.radius("lg"));
    drawList.AddRect(pos, max, ui.color("border/subtle"), ui.radius("lg"), ui.border());
    if (!station) {
        const float measure = std::min(size.x - 2.0F * ui.spacing("24"), 360.0F);
        const std::string_view text = ui.tr(StringId::NeedStation);
        const ImVec2 textSize = ui.textSize(TypeRole::Body, text, measure);
        ui.drawText(
            drawList, TypeRole::Body,
            ImVec2(pos.x + (size.x - textSize.x) * 0.5F, pos.y + (size.y - textSize.y) * 0.5F),
            ui.color("text/secondary"), text, measure);
        return;
    }

    // Range: the smallest that holds every aircraft.
    double farthest = 0.0;
    for (const auto& [icao, aircraft] : store.aircraft()) {
        if (aircraft.position) {
            farthest = std::max(farthest, adsb::distanceKm(*station, *aircraft.position));
        }
    }
    double range = kRanges.back();
    for (double r : kRanges) {
        if (farthest * 1.1 <= r) {
            range = r;
            break;
        }
    }
    const ImVec2 centre(pos.x + size.x * 0.5F, pos.y + size.y * 0.5F);
    const float radius = std::min(size.x, size.y) * 0.5F - ui.spacing("24");
    const auto scale = static_cast<float>(radius / range);
    auto project = [&](const adsb::LatLon& where) {
        const double d = adsb::distanceKm(*station, where);
        const double b = adsb::bearingDeg(*station, where) * std::numbers::pi / 180.0;
        return ImVec2(centre.x + static_cast<float>(d * std::sin(b)) * scale,
                      centre.y - static_cast<float>(d * std::cos(b)) * scale);
    };

    // Rings at quarters of the range, labelled; compass points.
    const ImU32 ring = ui.color("border/subtle");
    const ImU32 label = ui.color("text/secondary");
    for (int q = 1; q <= 4; ++q) {
        const float r = radius * static_cast<float>(q) / 4.0F;
        drawList.AddCircle(centre, r, ring, 0, ui.border());
        const std::string text = std::format("{:.0f} km", range * q / 4.0);
        ui.drawText(drawList, TypeRole::MonoSmall, ImVec2(centre.x + 4.0F, centre.y - r + 2.0F),
                    label, text);
    }
    drawList.AddLine(ImVec2(centre.x - radius, centre.y), ImVec2(centre.x + radius, centre.y), ring,
                     ui.border());
    drawList.AddLine(ImVec2(centre.x, centre.y - radius), ImVec2(centre.x, centre.y + radius), ring,
                     ui.border());
    const std::array<std::pair<std::string_view, ImVec2>, 4> compass{{
        {ui.tr(StringId::CompassNorth), ImVec2(centre.x, centre.y - radius - 14.0F)},
        {ui.tr(StringId::CompassEast), ImVec2(centre.x + radius + 8.0F, centre.y)},
        {ui.tr(StringId::CompassSouth), ImVec2(centre.x, centre.y + radius + 6.0F)},
        {ui.tr(StringId::CompassWest), ImVec2(centre.x - radius - 14.0F, centre.y)},
    }};
    for (const auto& [text, where] : compass) {
        const ImVec2 textSize = ui.textSize(TypeRole::Label, text);
        ui.drawText(drawList, TypeRole::Label,
                    ImVec2(where.x - textSize.x * 0.5F, where.y - textSize.y * 0.5F), label, text);
    }
    // The station.
    drawList.AddCircleFilled(centre, 3.0F, ui.color("text/primary"));

    // Aircraft: trails, then arrows and labels.
    const auto now = Clock::now();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    std::optional<uint32_t> hovered;
    float hoveredDistance = kPickRadius;
    drawList.PushClipRect(pos, max, true);
    for (const auto& [icao, aircraft] : store.aircraft()) {
        if (!aircraft.position) {
            continue;
        }
        const bool stale = now - aircraft.lastSeen > kStale;
        const bool selected = m_selected == icao;
        if (aircraft.trail.size() > 1) {
            std::vector<ImVec2> points;
            for (const auto& point : aircraft.trail) {
                points.push_back(project(point.position));
            }
            drawList.AddPolyline(points.data(), static_cast<int>(points.size()),
                                 ui.color(selected ? "accent" : "border/strong"), ui.border());
        }
        const ImVec2 at = project(*aircraft.position);
        const ImU32 color = ui.color(stale ? "text/disabled" : "accent");
        if (selected) {
            drawList.AddCircle(at, 11.0F, ui.color("accent"), 0,
                               ui.tokens().border("focus").value_or(2.0F));
        }
        if (aircraft.trackDeg) {
            arrow(drawList, at, selected ? 8.0F : 6.5F, *aircraft.trackDeg, color);
        } else {
            drawList.AddCircleFilled(at, 4.0F, color);
        }
        std::string text = nameOf(aircraft);
        if (aircraft.altitudeFt) {
            text += "\n" + formatThousands(*aircraft.altitudeFt) + " ft";
        }
        ui.drawText(drawList, TypeRole::MonoSmall, ImVec2(at.x + 10.0F, at.y - 8.0F),
                    ui.color(stale ? "text/disabled" : "text/primary"), text);
        const float d = std::hypot(mouse.x - at.x, mouse.y - at.y);
        if (d < hoveredDistance) {
            hoveredDistance = d;
            hovered = icao;
        }
    }
    drawList.PopClipRect();

    // Click an aircraft to select it.
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton("##radar", size);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_selected = hovered;
    }
}

}  // namespace zarescope::ui
