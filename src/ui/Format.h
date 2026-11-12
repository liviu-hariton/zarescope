// SPDX-License-Identifier: GPL-3.0-or-later
//
// Number formats from design/docs/DESIGN.md §3. Pure functions, no ImGui.
//
//   frequency   137.900 000 MHz   (6 decimals, thin space after the kHz group)
//   duration    04:32, 1:02:03
//   size        8.4 MB, 120 MB
//   rate        2.4 MS/s, 250 kS/s
//   thousands   36 000

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zarescope::ui {

// U+2009 THIN SPACE and U+2212 MINUS SIGN, UTF-8.
inline constexpr std::string_view kThinSpace = "\xE2\x80\x89";
inline constexpr std::string_view kMinus = "\xE2\x88\x92";

// One character of the frequency display.
struct FrequencyGlyph {
    std::string text;     // one UTF-8 character
    int64_t placeHz = 0;  // the digit's place value (1, 10, … 10⁹); 0 for separators
    bool dimmed = false;  // trailing zeros render in text/secondary
};

// "137.900 000" split into glyphs, without the unit. Negative values aren't
// frequencies and are clamped to 0.
std::vector<FrequencyGlyph> frequencyGlyphs(int64_t frequencyHz);

// "137.900 000 MHz".
std::string formatFrequency(int64_t frequencyHz);

// "04:32" under an hour, "1:02:03" from an hour.
std::string formatDuration(int64_t seconds);

// SI units (1 MB = 10⁶ bytes), one decimal below 10: "640 kB", "8.4 MB", "1.2 GB".
std::string formatBytes(uint64_t bytes);

// "2.4 MS/s", "2.048 MS/s", "250 kS/s".
std::string formatSampleRate(uint32_t sampleRateSps);

// "29.7 dB" from tenths of a dB.
std::string formatGain(int gainTenthsDb);

// A round axis step (1, 2 or 5 × 10ⁿ) giving about `targetTicks` ticks over
// `span`.
double niceStep(double span, int targetTicks);

// Decimals needed to tell ticks `step` apart: 0.25 → 2, 0.5 → 1, 2 → 0.
int decimalsFor(double step);

// "36 000" with a thin space between thousands and a true minus sign.
std::string formatThousands(int value);

// "−42.3" with a true minus sign (DESIGN.md §3), `decimals` places.
std::string formatSigned(double value, int decimals);

}  // namespace zarescope::ui
