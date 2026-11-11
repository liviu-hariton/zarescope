// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/WaterfallColors.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace zarescope::ui {

PackedLut packLut(const std::array<theme::Rgba, 256>& lut) {
    PackedLut packed{};
    auto byte = [](float channel) {
        return static_cast<uint32_t>(std::lround(std::clamp(channel, 0.0F, 1.0F) * 255.0F));
    };
    for (size_t i = 0; i < lut.size(); ++i) {
        packed[i] = byte(lut[i].r) | (byte(lut[i].g) << 8) | (byte(lut[i].b) << 16) | (255U << 24);
    }
    return packed;
}

uint8_t lutIndex(float dbfs, float floorDb, float ceilingDb) {
    const float span = std::max(ceilingDb - floorDb, 1e-3F);
    const float t = std::clamp((dbfs - floorDb) / span, 0.0F, 1.0F);
    return static_cast<uint8_t>(std::lround(t * 255.0F));
}

void resampleRow(std::span<const float> dbfs, std::span<float> out) {
    const size_t bins = dbfs.size();
    const size_t width = out.size();
    if (bins == 0) {
        std::fill(out.begin(), out.end(), -1000.0F);
        return;
    }
    for (size_t x = 0; x < width; ++x) {
        // Bins [first, last) fall in column x.
        const size_t first = x * bins / width;
        const size_t last = std::max(first + 1, (x + 1) * bins / width);
        float level = dbfs[first];
        for (size_t b = first + 1; b < last && b < bins; ++b) {
            level = std::max(level, dbfs[b]);
        }
        out[x] = level;
    }
}

void colorizeLevels(std::span<const float> levels, float floorDb, float ceilingDb,
                    const PackedLut& lut, std::span<uint32_t> pixels) {
    for (size_t x = 0; x < pixels.size() && x < levels.size(); ++x) {
        pixels[x] = lut[lutIndex(levels[x], floorDb, ceilingDb)];
    }
}

void colorizeRow(std::span<const float> dbfs, float floorDb, float ceilingDb, const PackedLut& lut,
                 std::span<uint32_t> pixels) {
    std::vector<float> levels(pixels.size());
    resampleRow(dbfs, levels);
    colorizeLevels(levels, floorDb, ceilingDb, lut, pixels);
}

LevelRange suggestRange(std::span<const float> dbfs) {
    if (dbfs.empty()) {
        return {-120.0F, -20.0F};
    }
    std::vector<float> sorted(dbfs.begin(), dbfs.end());
    const auto noiseAt = sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 5);
    std::nth_element(sorted.begin(), noiseAt, sorted.end());
    const float noise = *noiseAt;
    const float peak = *std::max_element(dbfs.begin(), dbfs.end());
    float floor = std::floor((noise - 10.0F) / 5.0F) * 5.0F;
    float ceiling = std::min(0.0F, std::ceil((peak + 5.0F) / 5.0F) * 5.0F);
    if (ceiling - floor < 30.0F) {
        floor = ceiling - 30.0F;
    }
    return {floor, ceiling};
}

}  // namespace zarescope::ui
