// SPDX-License-Identifier: GPL-3.0-or-later
//
// Turning spectrum rows into waterfall pixels. Pure functions, no GPU, so
// the mapping is tested on its own.
//
// Each row of dBFS values becomes one line of RGBA pixels:
//   1. resample the FFT bins to the texture width: when bins outnumber
//      pixels, each pixel takes the strongest bin it covers (so a narrow
//      carrier never disappears); when pixels outnumber bins, the nearest bin
//   2. map floor…ceiling (dBFS) to LUT index 0…255, clamped
//   3. look up the "Zare Deep" colour (design/tokens, the same in both themes)

#pragma once

#include "ui/theme/DesignTokens.h"

#include <array>
#include <cstdint>
#include <span>

namespace zarescope::ui {

// ImGui's RGBA32 texture layout: R in the lowest byte (IM_COL32 order).
using PackedLut = std::array<uint32_t, 256>;

PackedLut packLut(const std::array<theme::Rgba, 256>& lut);

// The LUT index for a level: floor → 0, ceiling → 255, clamped.
uint8_t lutIndex(float dbfs, float floorDb, float ceilingDb);

// Step 1 alone: resamples `dbfs` to `out.size()` columns (max when shrinking,
// nearest when stretching).
void resampleRow(std::span<const float> dbfs, std::span<float> out);

// Steps 2–3 alone: one level per pixel → colours.
void colorizeLevels(std::span<const float> levels, float floorDb, float ceilingDb,
                    const PackedLut& lut, std::span<uint32_t> pixels);

// All three steps: fills `pixels` (one texture row) from `dbfs` (one row).
void colorizeRow(std::span<const float> dbfs, float floorDb, float ceilingDb, const PackedLut& lut,
                 std::span<uint32_t> pixels);

// A display range that fits a spectrum: the floor a little under the noise
// (its 20th percentile), the ceiling a little over the strongest signal,
// both on 5-dB steps, at least 30 dB apart, the ceiling at most 0 dBFS.
struct LevelRange {
    float floorDb;
    float ceilingDb;
};
LevelRange suggestRange(std::span<const float> dbfs);

}  // namespace zarescope::ui
