// SPDX-License-Identifier: GPL-3.0-or-later
//
// The UI's pure helpers: number formats (DESIGN.md §3), the units chosen in
// Settings, and the waterfall's row-to-pixel mapping.

#include "ui/Format.h"
#include "ui/Units.h"
#include "ui/WaterfallColors.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

using namespace zarescope::ui;

namespace {

const std::string kThin(kThinSpace);

std::string dimmedPart(const std::vector<FrequencyGlyph>& glyphs) {
    std::string out;
    for (const auto& glyph : glyphs) {
        if (glyph.dimmed) {
            out += glyph.text;
        }
    }
    return out;
}

}  // namespace

TEST_CASE("format: frequencies read 137.900 000 MHz") {
    CHECK(formatFrequency(137'900'000) == "137.900" + kThin + "000 MHz");
    CHECK(formatFrequency(1'090'000'000) == "1090.000" + kThin + "000 MHz");
    CHECK(formatFrequency(98'123'456) == "98.123" + kThin + "456 MHz");
    CHECK(formatFrequency(500'000) == "0.500" + kThin + "000 MHz");
    CHECK(formatFrequency(-5) == "0.000" + kThin + "000 MHz");
}

TEST_CASE("format: frequency glyphs carry place values and dim the trailing zeros") {
    const auto glyphs = frequencyGlyphs(137'900'000);
    // 1 3 7 . 9 0 0 ␣ 0 0 0
    REQUIRE(glyphs.size() == 11);
    CHECK(glyphs[0].placeHz == 100'000'000);
    CHECK(glyphs[2].placeHz == 1'000'000);
    CHECK(glyphs[3].placeHz == 0);  // the point
    CHECK(glyphs[4].placeHz == 100'000);
    CHECK(glyphs[7].placeHz == 0);  // the thin space
    CHECK(glyphs[10].placeHz == 1);
    CHECK(dimmedPart(glyphs) == "00" + kThin + "000");
    CHECK_FALSE(glyphs[4].dimmed);  // the 9

    // All decimals zero: the point dims too.
    CHECK(dimmedPart(frequencyGlyphs(100'000'000)) == ".000" + kThin + "000");
    // No trailing zeros: nothing dims.
    CHECK(dimmedPart(frequencyGlyphs(98'123'457)).empty());
}

TEST_CASE("format: durations, sizes, rates and gains") {
    CHECK(formatDuration(0) == "00:00");
    CHECK(formatDuration(272) == "04:32");
    CHECK(formatDuration(3723) == "1:02:03");
    CHECK(formatBytes(512) == "512 B");
    CHECK(formatBytes(640'000) == "640 kB");
    CHECK(formatBytes(8'400'000) == "8.4 MB");
    CHECK(formatBytes(120'000'000) == "120 MB");
    CHECK(formatBytes(1'200'000'000) == "1.2 GB");
    CHECK(formatSampleRate(2'400'000) == "2.4 MS/s");
    CHECK(formatSampleRate(2'048'000) == "2.048 MS/s");
    CHECK(formatSampleRate(3'200'000) == "3.2 MS/s");
    CHECK(formatSampleRate(250'000) == "250 kS/s");
    CHECK(formatGain(297) == "29.7 dB");
    CHECK(formatGain(0) == "0.0 dB");
    CHECK(formatGain(-10) == "-1.0 dB");
}

TEST_CASE("format: axis ticks are round and signed values use a true minus") {
    CHECK(niceStep(2.4, 12) == doctest::Approx(0.2));
    CHECK(niceStep(2.4, 6) == doctest::Approx(0.5));
    CHECK(niceStep(100.0, 5) == doctest::Approx(20.0));
    CHECK(niceStep(1.0, 4) == doctest::Approx(0.2));
    CHECK(niceStep(0.25, 5) == doctest::Approx(0.05));
    CHECK(decimalsFor(0.2) == 1);
    CHECK(decimalsFor(0.05) == 2);
    CHECK(decimalsFor(20.0) == 0);
    CHECK(formatSigned(-42.3, 1) == std::string(kMinus) + "42.3");
    CHECK(formatSigned(-0.01, 1) == "0.0");  // no "−0.0"
    CHECK(formatSigned(12.0, 0) == "12");
}

TEST_CASE("format: thousands are split by a thin space, negatives use a true minus") {
    CHECK(formatThousands(0) == "0");
    CHECK(formatThousands(950) == "950");
    CHECK(formatThousands(36'000) == "36" + kThin + "000");
    CHECK(formatThousands(1'234'567) == "1" + kThin + "234" + kThin + "567");
    CHECK(formatThousands(-1000) == std::string(kMinus) + "1" + kThin + "000");
}

TEST_CASE("units: the settings key round-trips, unknown keys read as metric") {
    for (UnitSystem system : {UnitSystem::Metric, UnitSystem::Aviation, UnitSystem::Imperial}) {
        CHECK(unitSystemFromKey(unitSystemKey(system)) == system);
    }
    CHECK(unitSystemFromKey("") == UnitSystem::Metric);
    CHECK(unitSystemFromKey("furlongs") == UnitSystem::Metric);
}

TEST_CASE("units: altitude, speed and distance in each system") {
    // 36 000 ft = 10 972.8 m; 452 kt = 837.1 km/h = 520.2 mph; 18.52 km = 10 NM.
    auto text = [](const Measurement& m) { return m.value + " " + std::string(m.unit); };
    CHECK(text(formatAltitude(36'000, UnitSystem::Metric)) == "10" + kThin + "973 m");
    CHECK(text(formatAltitude(36'000, UnitSystem::Aviation)) == "36" + kThin + "000 ft");
    CHECK(text(formatAltitude(36'000, UnitSystem::Imperial)) == "36" + kThin + "000 ft");
    CHECK(text(formatAltitude(-1000, UnitSystem::Metric)) == std::string(kMinus) + "305 m");
    CHECK(text(formatSpeed(452.0, UnitSystem::Metric)) == "837 km/h");
    CHECK(text(formatSpeed(452.0, UnitSystem::Aviation)) == "452 kt");
    CHECK(text(formatSpeed(452.0, UnitSystem::Imperial)) == "520 mph");
    CHECK(text(formatDistance(18.52, UnitSystem::Metric)) == "18.5 km");
    CHECK(text(formatDistance(18.52, UnitSystem::Aviation)) == "10.0 NM");
    CHECK(text(formatDistance(16.09344, UnitSystem::Imperial)) == "10.0 mi");
}

TEST_CASE("units: station height, the Settings summary and the radar ranges") {
    CHECK(heightUnit(UnitSystem::Metric) == "m");
    CHECK(heightUnit(UnitSystem::Aviation) == "ft");
    CHECK(metresPerHeightUnit(UnitSystem::Imperial) == doctest::Approx(0.3048));
    CHECK(unitSummary(UnitSystem::Metric) == "m · km/h · km");
    CHECK(unitSummary(UnitSystem::Aviation) == "ft · kt · NM");
    CHECK(unitSummary(UnitSystem::Imperial) == "ft · mph · mi");
    for (UnitSystem system : {UnitSystem::Metric, UnitSystem::Aviation, UnitSystem::Imperial}) {
        const auto ranges = radarRanges(system);
        REQUIRE_FALSE(ranges.empty());
        for (size_t i = 0; i < ranges.size(); ++i) {
            // Rings at quarters of the range get whole-number labels.
            CHECK(std::fmod(ranges[i], 4.0) == 0.0);
            if (i > 0) {
                CHECK(ranges[i] > ranges[i - 1]);
            }
        }
        // Every system reaches about the same distance (≈ 400 km).
        const double farthestKm = ranges.back() * kmPerDistanceUnit(system);
        CHECK(farthestKm > 380.0);
        CHECK(farthestKm < 420.0);
    }
}

TEST_CASE("waterfall: levels map floor → 0 and ceiling → 255, clamped") {
    CHECK(lutIndex(-120.0F, -120.0F, -20.0F) == 0);
    CHECK(lutIndex(-20.0F, -120.0F, -20.0F) == 255);
    CHECK(lutIndex(-70.0F, -120.0F, -20.0F) == 128);
    CHECK(lutIndex(-200.0F, -120.0F, -20.0F) == 0);
    CHECK(lutIndex(10.0F, -120.0F, -20.0F) == 255);
    CHECK(lutIndex(-50.0F, -50.0F, -50.0F) == 0);  // degenerate range doesn't divide by zero
}

TEST_CASE("waterfall: rows resample without losing narrow carriers") {
    PackedLut lut{};
    for (uint32_t i = 0; i < 256; ++i) {
        lut[i] = i;  // the index itself, to read back
    }
    // 16 bins into 4 pixels: one strong bin per group must win its pixel.
    std::vector<float> row(16, -120.0F);
    row[5] = -20.0F;
    std::vector<uint32_t> pixels(4);
    colorizeRow(row, -120.0F, -20.0F, lut, pixels);
    CHECK(pixels == std::vector<uint32_t>{0, 255, 0, 0});

    // 4 bins into 8 pixels: each bin covers two pixels.
    std::vector<float> few{-120.0F, -70.0F, -20.0F, -120.0F};
    std::vector<uint32_t> wide(8);
    colorizeRow(few, -120.0F, -20.0F, lut, wide);
    CHECK(wide == std::vector<uint32_t>{0, 0, 128, 128, 255, 255, 0, 0});
}

TEST_CASE("waterfall: the packed LUT is RGBA in ImGui's byte order, opaque") {
    std::array<zarescope::theme::Rgba, 256> lut{};
    lut[0] = {1.0F, 0.0F, 0.0F, 1.0F};
    lut[255] = {0.0F, 0.0F, 1.0F, 1.0F};
    const auto packed = packLut(lut);
    CHECK(packed[0] == 0xFF0000FFU);    // A B G R → red
    CHECK(packed[255] == 0xFFFF0000U);  // blue
}

TEST_CASE("waterfall: the suggested range sits around the noise and the peak") {
    std::vector<float> row(1000, -82.0F);
    row[500] = -31.0F;
    const auto range = suggestRange(row);
    CHECK(range.floorDb == doctest::Approx(-95.0));    // −82 − 10 → −92 → down to −95
    CHECK(range.ceilingDb == doctest::Approx(-25.0));  // −31 + 5 → −26 → up to −25
    // A flat spectrum still gets a 30-dB window.
    const auto flat = suggestRange(std::vector<float>(100, -70.0F));
    CHECK(flat.ceilingDb - flat.floorDb >= 30.0F);
    // Never above 0 dBFS.
    CHECK(suggestRange(std::vector<float>(10, -1.0F)).ceilingDb <= 0.0F);
}
