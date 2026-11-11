// SPDX-License-Identifier: GPL-3.0-or-later
//
// SpectrumView: the spectrum plot above the waterfall (DESIGN.md §6).
//
//   ┌ spectrum (ImPlot, 210 px): accent trace over accent/soft fill,
//   │ dashed marker at the tuned frequency, dBFS axis on the left
//   ├ waterfall: newest row at the top, "Zare Deep" colours, UTC time
//   │ labels on the left
//   └ frequency axis (MHz)
//
// The waterfall is a ring texture: each new row overwrites the oldest and
// is uploaded alone (one row of pixels, not the whole texture), through
// Dear ImGui's user textures, so no graphics-API code is needed here. The
// spectrum and the waterfall share the same pixel columns, so a carrier
// lines up in both.
//
// Clicking the spectrum or the waterfall moves the VFO (the listening
// frequency) there; the scroll wheel over them steps it. The caller decides
// whether that needs a retune of the stick.

#pragma once

#include "device/Receiver.h"
#include "ui/UiKit.h"
#include "ui/WaterfallColors.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct ImTextureData;

namespace zarescope::ui {

// The listening frequency and its channel, as drawn over the spectrum.
struct Vfo {
    int64_t frequencyHz = 0;
    double bandwidthHz = 0.0;  // 0: no channel drawn (no mode)
    int64_t snapHz = 1'000;    // clicks land on multiples of this
    int64_t stepHz = 100'000;  // one notch of the scroll wheel
};

class SpectrumView {
public:
    // Needs a live ImGui context (it registers the waterfall texture).
    explicit SpectrumView(const theme::DesignTokens& tokens);
    ~SpectrumView();
    SpectrumView(const SpectrumView&) = delete;
    SpectrumView& operator=(const SpectrumView&) = delete;
    SpectrumView(SpectrumView&&) = delete;
    SpectrumView& operator=(SpectrumView&&) = delete;

    // Feeds data. Rows from a different frequency or rate than the last
    // clear the waterfall: old rows would no longer line up.
    void addRow(const device::SpectrumRow& row);
    void setTrace(const device::SpectrumRow& trace);
    // Forgets everything shown; the next source's first trace sets the
    // level range.
    void clear();
    // Sets floorDb/ceilingDb to fit the current trace (WaterfallColors.h).
    void fitRange();

    // Draws into the rectangle [pos, pos + size]. Returns the new VFO
    // frequency when the user clicked or scrolled.
    std::optional<int64_t> draw(UiKit& ui, ImVec2 pos, ImVec2 size, const Vfo& vfo);

    // dBFS range of the waterfall colours and the spectrum's vertical axis.
    float floorDb = -120.0F;
    float ceilingDb = -20.0F;

private:
    void repaintAll();
    void clearWaterfall();

    PackedLut m_lut;
    std::unique_ptr<ImTextureData> m_texture;
    int m_head = 0;    // texture row of the newest line
    int m_filled = 0;  // lines written since the last clear
    std::vector<uint32_t> m_rowPixels;
    std::vector<std::chrono::system_clock::time_point> m_rowTimes;  // per texture row
    // The rows' dBFS values, kept to repaint when the range changes.
    std::vector<std::vector<float>> m_rowLevels;
    float m_paintedFloor = 0.0F;
    float m_paintedCeiling = 0.0F;

    device::SpectrumRow m_trace;
    bool m_fitPending = true;  // fit the range to the first trace of a new source
    int64_t m_centerHz = 0;
    uint32_t m_rateSps = 0;
};

}  // namespace zarescope::ui
