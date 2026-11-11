// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/SpectrumView.h"

#include "ui/Clock.h"
#include "ui/Format.h"

#include <imgui_internal.h>  // RegisterUserTexture, ImTextureDataQueueUpload (user textures)
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace zarescope::ui {

namespace {

// Texture size: 2048 columns (bins are max-resampled to it), 1024 rows
// (34 s of history at 30 rows/s, one row per point on screen).
constexpr int kWidth = 2048;
constexpr int kHeight = 1024;
// DESIGN.md §6: the spectrum plot is 210 px tall.
constexpr float kSpectrumHeight = 210.0F;
// One notch of the scroll wheel over the spectrum steps this far.
constexpr int64_t kWheelStepHz = 100'000;
// Clicks tune to whole kilohertz.
constexpr int64_t kClickResolutionHz = 1'000;
// Waterfall time labels: about every 64 points.
constexpr float kTimeLabelSpacing = 64.0F;

// Tick positions: multiples of `step` within [lo, hi]. Counted with integers
// and capped, so odd ranges (a huge frequency where `step` is below the
// floating-point resolution) can never loop forever.
std::vector<double> axisTicks(double lo, double hi, double step) {
    constexpr int64_t kMaxTicks = 64;
    std::vector<double> ticks;
    if (!std::isfinite(lo) || !std::isfinite(hi) || !std::isfinite(step) || step <= 0.0 ||
        hi < lo) {
        return ticks;
    }
    const double first = std::ceil(lo / step);
    const double last = std::floor((hi + 1e-9) / step);
    if (!std::isfinite(first) || !std::isfinite(last) || last - first > kMaxTicks) {
        return ticks;
    }
    for (auto i = static_cast<int64_t>(first); i <= static_cast<int64_t>(last); ++i) {
        ticks.push_back(static_cast<double>(i) * step);
    }
    return ticks;
}

}  // namespace

SpectrumView::SpectrumView(const theme::DesignTokens& tokens)
    : m_lut(packLut(tokens.waterfallLut())),
      m_texture(std::make_unique<ImTextureData>()),
      m_rowPixels(kWidth),
      m_rowTimes(kHeight),
      m_rowLevels(kHeight, std::vector<float>(kWidth, -1000.0F)) {
    m_texture->Create(ImTextureFormat_RGBA32, kWidth, kHeight);
    ImGui::RegisterUserTexture(m_texture.get());
    m_paintedFloor = floorDb;
    m_paintedCeiling = ceilingDb;
    clear();
}

SpectrumView::~SpectrumView() {
    // The renderer releases the GPU copy when it shuts down; this only takes
    // the texture off ImGui's list. Needs the ImGui context, so the app
    // destroys views before the context.
    ImGui::UnregisterUserTexture(m_texture.get());
}

void SpectrumView::clear() {
    clearWaterfall();
    m_trace.dbfs.clear();
    m_fitPending = true;
}

void SpectrumView::clearWaterfall() {
    for (auto& levels : m_rowLevels) {
        std::fill(levels.begin(), levels.end(), -1000.0F);
    }
    auto* pixels = static_cast<uint32_t*>(m_texture->GetPixels());
    std::fill(pixels, pixels + static_cast<size_t>(kWidth) * kHeight, m_lut[0]);
    m_head = 0;
    m_filled = 0;
    if (m_texture->Status != ImTextureStatus_WantCreate) {
        ImTextureDataQueueUpload(m_texture.get(), 0, 0, kWidth, kHeight);
    }
}

void SpectrumView::fitRange() {
    if (m_trace.dbfs.empty()) {
        return;
    }
    const LevelRange range = suggestRange(m_trace.dbfs);
    floorDb = range.floorDb;
    ceilingDb = range.ceilingDb;
}

void SpectrumView::repaintAll() {
    for (int row = 0; row < kHeight; ++row) {
        auto* pixels = static_cast<uint32_t*>(m_texture->GetPixelsAt(0, row));
        colorizeLevels(m_rowLevels[static_cast<size_t>(row)], floorDb, ceilingDb, m_lut,
                       std::span<uint32_t>(pixels, kWidth));
    }
    if (m_texture->Status != ImTextureStatus_WantCreate) {
        ImTextureDataQueueUpload(m_texture.get(), 0, 0, kWidth, kHeight);
    }
    m_paintedFloor = floorDb;
    m_paintedCeiling = ceilingDb;
}

void SpectrumView::addRow(const device::SpectrumRow& row) {
    if (row.centerFrequencyHz != m_centerHz || row.sampleRateSps != m_rateSps) {
        m_centerHz = row.centerFrequencyHz;
        m_rateSps = row.sampleRateSps;
        clearWaterfall();  // a retune keeps the user's level range
    }
    // Newest row goes one above the previous newest (wrapping), so reading
    // the texture downward from m_head runs newest → oldest.
    m_head = (m_head - 1 + kHeight) % kHeight;
    m_filled = std::min(m_filled + 1, kHeight);
    auto& levels = m_rowLevels[static_cast<size_t>(m_head)];
    resampleRow(row.dbfs, levels);
    auto* pixels = static_cast<uint32_t*>(m_texture->GetPixelsAt(0, m_head));
    colorizeLevels(levels, floorDb, ceilingDb, m_lut, std::span<uint32_t>(pixels, kWidth));
    m_rowTimes[static_cast<size_t>(m_head)] = row.timeUtc;
    if (m_texture->Status != ImTextureStatus_WantCreate) {
        ImTextureDataQueueUpload(m_texture.get(), 0, m_head, kWidth, 1);
    }
}

void SpectrumView::setTrace(const device::SpectrumRow& trace) {
    m_trace.dbfs.assign(trace.dbfs.begin(), trace.dbfs.end());
    m_trace.centerFrequencyHz = trace.centerFrequencyHz;
    m_trace.sampleRateSps = trace.sampleRateSps;
    if (m_fitPending && !m_trace.dbfs.empty()) {
        fitRange();
        m_fitPending = false;
    }
}

std::optional<int64_t> SpectrumView::draw(UiKit& ui, ImVec2 pos, ImVec2 size, int64_t tunedHz,
                                          bool canTune) {
    if (floorDb != m_paintedFloor || ceilingDb != m_paintedCeiling) {
        repaintAll();
    }
    std::optional<int64_t> tuneTo;
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const auto& small = ui.face(TypeRole::MonoSmall);
    const float pad = ui.spacing("8");
    const float gutter = ui.textSize(TypeRole::MonoSmall, "00:00:00").x + 2.0F * pad;
    const float axisHeight = ui.textSize(TypeRole::MonoSmall, "0").y + 2.0F * pad;
    const ImU32 axisText = ui.color("text/secondary");

    const double spanMHz = m_rateSps / 1e6;
    const double centerMHz = static_cast<double>(m_centerHz) / 1e6;
    const double lo = centerMHz - spanMHz / 2.0;
    const double hi = centerMHz + spanMHz / 2.0;
    const bool hasData = m_rateSps > 0;

    const float plotX = pos.x + gutter;
    const float plotW = std::max(1.0F, size.x - gutter - pad);
    const float spectrumH = std::min(kSpectrumHeight, size.y * 0.45F);
    const float waterfallTop = pos.y + spectrumH + 1.0F;
    const float waterfallH = std::max(0.0F, size.y - spectrumH - axisHeight - 1.0F);

    auto xToHz = [&](float x) {
        const double mhz = lo + (static_cast<double>(x - plotX) / plotW) * (hi - lo);
        const auto hz =
            static_cast<int64_t>(std::llround(mhz * 1e6 / kClickResolutionHz)) * kClickResolutionHz;
        return hz;
    };
    auto hzToX = [&](double hz) {
        return plotX + static_cast<float>((hz / 1e6 - lo) / (hi - lo)) * plotW;
    };

    // Shared x ticks for the grid and the axis labels.
    const double step = niceStep(hi - lo, std::max(2, static_cast<int>(plotW / 110.0F)));
    const std::vector<double> ticks = hasData ? axisTicks(lo, hi, step) : std::vector<double>{};
    const double dbStep =
        niceStep(ceilingDb - floorDb, std::max(2, static_cast<int>(spectrumH / 40.0F)));
    const std::vector<double> dbTicks = axisTicks(floorDb, ceilingDb, dbStep);

    // --- Spectrum (ImPlot) ----------------------------------------------------
    ImGui::SetCursorScreenPos(ImVec2(plotX, pos.y));
    ImGui::PushFont(small.font, small.sizePx);
    constexpr ImPlotFlags kPlotFlags =
        ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs;
    constexpr ImPlotAxisFlags kAxisFlags =
        ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoMenus |
        ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoLabel;
    bool plotHovered = false;
    ImVec2 mouse = ImGui::GetIO().MousePos;
    if (ImPlot::BeginPlot("##spectrum", ImVec2(plotW, spectrumH), kPlotFlags)) {
        ImPlot::SetupAxes(nullptr, nullptr, kAxisFlags, kAxisFlags);
        ImPlot::SetupAxisLimits(ImAxis_X1, hasData ? lo : 0.0, hasData ? hi : 1.0,
                                ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, floorDb, ceilingDb, ImPlotCond_Always);
        if (!ticks.empty()) {
            ImPlot::SetupAxisTicks(ImAxis_X1, ticks.data(), static_cast<int>(ticks.size()));
        }
        ImPlot::SetupAxisTicks(ImAxis_Y1, dbTicks.data(), static_cast<int>(dbTicks.size()));

        if (hasData && !m_trace.dbfs.empty() && m_trace.sampleRateSps == m_rateSps &&
            m_trace.centerFrequencyHz == m_centerHz) {
            const int n = static_cast<int>(m_trace.dbfs.size());
            const double xscale = spanMHz / n;
            ImPlotSpec fill;
            fill.FillColor = ui.colorVec("accent/soft");
            ImPlot::PlotShaded("##fill", m_trace.dbfs.data(), n, floorDb, xscale, lo, fill);
            ImPlotSpec line;
            line.LineColor = ui.colorVec("accent");
            line.LineWeight = 1.5F;
            ImPlot::PlotLine("##trace", m_trace.dbfs.data(), n, xscale, lo, line);
        }
        plotHovered = ImPlot::IsPlotHovered();
        ImPlot::EndPlot();
    }
    ImGui::PopFont();

    // dBFS labels in the gutter, right-aligned against the plot, under the
    // unit label at the top.
    const ImVec2 unitSize = ui.textSize(TypeRole::MonoSmall, "dBFS");
    ui.drawText(drawList, TypeRole::MonoSmall, ImVec2(pos.x + pad, pos.y), axisText, "dBFS");
    for (double t : dbTicks) {
        const float y =
            pos.y + static_cast<float>((ceilingDb - t) / (ceilingDb - floorDb)) * spectrumH;
        const std::string label = formatSigned(t, 0);
        const ImVec2 labelSize = ui.textSize(TypeRole::MonoSmall, label);
        const float labelY =
            std::clamp(y - labelSize.y * 0.5F, pos.y, pos.y + spectrumH - labelSize.y);
        if (labelY < pos.y + unitSize.y) {
            continue;  // would overlap the unit
        }
        ui.drawText(drawList, TypeRole::MonoSmall, ImVec2(plotX - pad - labelSize.x, labelY),
                    axisText, label);
    }

    // Tuned-frequency marker: dashed accent line through the spectrum.
    if (hasData && tunedHz >= static_cast<int64_t>(lo * 1e6) &&
        tunedHz <= static_cast<int64_t>(hi * 1e6)) {
        const float x = hzToX(static_cast<double>(tunedHz));
        for (float y = pos.y; y < pos.y + spectrumH; y += 8.0F) {
            drawList.AddLine(ImVec2(x, y), ImVec2(x, std::min(y + 4.0F, pos.y + spectrumH)),
                             ui.color("accent"), ui.border());
        }
    }

    // --- Waterfall ------------------------------------------------------------
    const ImVec2 wfMin(plotX, waterfallTop);
    const ImVec2 wfMax(plotX + plotW, waterfallTop + waterfallH);
    const ImU32 empty = m_lut[0];
    drawList.AddRectFilled(wfMin, wfMax, empty);
    const int visible = std::min({m_filled, kHeight, static_cast<int>(waterfallH)});
    if (visible > 0) {
        const float rowH = 1.0F;  // one row per point
        const int first = std::min(visible, kHeight - m_head);
        const auto tex = m_texture->GetTexRef();
        const float v0 = static_cast<float>(m_head) / kHeight;
        drawList.AddImage(tex, wfMin, ImVec2(wfMax.x, wfMin.y + first * rowH), ImVec2(0.0F, v0),
                          ImVec2(1.0F, static_cast<float>(m_head + first) / kHeight));
        if (visible > first) {
            const float y = wfMin.y + first * rowH;
            drawList.AddImage(tex, ImVec2(wfMin.x, y),
                              ImVec2(wfMax.x, y + (visible - first) * rowH), ImVec2(0.0F, 0.0F),
                              ImVec2(1.0F, static_cast<float>(visible - first) / kHeight));
        }
        // UTC time labels on the left, every ~64 points.
        for (float y = 0.0F; y < static_cast<float>(visible); y += kTimeLabelSpacing) {
            const int row = (m_head + static_cast<int>(y)) % kHeight;
            const std::string label =
                formatClock(m_rowTimes[static_cast<size_t>(row)], ClockZone::Utc);
            ui.drawText(drawList, TypeRole::MonoSmall, ImVec2(pos.x + pad, wfMin.y + y), axisText,
                        label);
        }
    }
    drawList.AddRect(ImVec2(plotX, pos.y), wfMax, ui.color("border/subtle"), 0.0F, ui.border());

    // --- Frequency axis ---------------------------------------------------------
    const float axisY = wfMax.y + pad;
    const int decimals = decimalsFor(step);
    for (double t : ticks) {
        const float x = hzToX(t * 1e6);
        drawList.AddLine(ImVec2(x, wfMax.y), ImVec2(x, wfMax.y + pad * 0.5F),
                         ui.color("border/strong"), ui.border());
        const std::string label = formatSigned(t, decimals);
        const float w = ui.textSize(TypeRole::MonoSmall, label).x;
        const float labelX = std::clamp(x - w * 0.5F, plotX, plotX + plotW - w);
        ui.drawText(drawList, TypeRole::MonoSmall, ImVec2(labelX, axisY), axisText, label);
    }
    if (hasData) {
        ui.drawText(drawList, TypeRole::MonoSmall, ImVec2(pos.x + pad, axisY), axisText, "MHz");
    }

    // --- Interaction ------------------------------------------------------------
    ImGui::SetCursorScreenPos(wfMin);
    ImGui::InvisibleButton("##waterfall", ImVec2(plotW, std::max(1.0F, waterfallH)));
    const bool waterfallHovered = ImGui::IsItemHovered();
    const bool hovered = hasData && (plotHovered || waterfallHovered) && mouse.x >= plotX &&
                         mouse.x <= plotX + plotW;
    if (hovered) {
        const int64_t hz = xToHz(mouse.x);
        // Level under the cursor, from the trace.
        std::string readout = formatFrequency(hz);
        if (!m_trace.dbfs.empty()) {
            const auto bin =
                static_cast<size_t>(std::clamp(static_cast<double>(mouse.x - plotX) / plotW *
                                                   static_cast<double>(m_trace.dbfs.size()),
                                               0.0, static_cast<double>(m_trace.dbfs.size() - 1)));
            readout += " · " + formatSigned(m_trace.dbfs[bin], 1) + " dBFS";
        }
        ImGui::BeginTooltip();
        ImGui::PushFont(ui.face(TypeRole::MonoData).font, ui.face(TypeRole::MonoData).sizePx);
        ImGui::TextUnformatted(readout.c_str());
        ImGui::PopFont();
        if (canTune) {
            ImGui::TextDisabled("%s", std::string(ui.tr(StringId::TuneHint)).c_str());
        }
        ImGui::EndTooltip();
        // Cursor line through both views.
        drawList.AddLine(ImVec2(mouse.x, pos.y), ImVec2(mouse.x, wfMax.y),
                         ui.color("text/secondary"), ui.border());

        if (canTune) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                tuneTo = hz;
            }
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0F) {
                tuneTo = tunedHz + static_cast<int64_t>(std::lround(wheel)) * kWheelStepHz;
            }
        }
    }
    return tuneTo;
}

}  // namespace zarescope::ui
