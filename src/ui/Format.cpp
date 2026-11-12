// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/Format.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>

namespace zarescope::ui {

std::vector<FrequencyGlyph> frequencyGlyphs(int64_t frequencyHz) {
    const int64_t hz = std::max<int64_t>(frequencyHz, 0);
    const int64_t mhz = hz / 1'000'000;
    const int64_t fraction = hz % 1'000'000;  // the six decimals

    std::vector<FrequencyGlyph> glyphs;
    // Integer MHz, at its natural width.
    const std::string whole = std::to_string(mhz);
    int64_t place = 1'000'000;
    for (size_t i = 1; i < whole.size(); ++i) {
        place *= 10;
    }
    for (char digit : whole) {
        glyphs.push_back({std::string(1, digit), place, false});
        place /= 10;
    }
    glyphs.push_back({".", 0, false});
    // Six decimals: kHz group, thin space, Hz group.
    const std::string decimals = std::format("{:06}", fraction);
    place = 100'000;
    for (size_t i = 0; i < 6; ++i) {
        if (i == 3) {
            glyphs.push_back({std::string(kThinSpace), 0, false});
        }
        glyphs.push_back({std::string(1, decimals[i]), place, false});
        place /= 10;
    }
    // Dim the trailing zeros of the decimals (and the separators among them).
    for (auto it = glyphs.rbegin(); it != glyphs.rend(); ++it) {
        if (it->placeHz == 0 && it->text == ".") {
            it->dimmed = std::all_of(it.base(), glyphs.end(),
                                     [](const FrequencyGlyph& g) { return g.dimmed; });
            break;
        }
        if (it->placeHz != 0 && it->text != "0") {
            break;
        }
        it->dimmed = true;
    }
    return glyphs;
}

std::string formatFrequency(int64_t frequencyHz) {
    std::string text;
    for (const auto& glyph : frequencyGlyphs(frequencyHz)) {
        text += glyph.text;
    }
    return text + " MHz";
}

std::string formatDuration(int64_t seconds) {
    seconds = std::max<int64_t>(seconds, 0);
    const int64_t h = seconds / 3600;
    const int64_t m = (seconds / 60) % 60;
    const int64_t s = seconds % 60;
    if (h > 0) {
        return std::format("{}:{:02}:{:02}", h, m, s);
    }
    return std::format("{:02}:{:02}", m, s);
}

std::string formatBytes(uint64_t bytes) {
    struct Unit {
        double size;
        const char* name;
    };
    constexpr Unit kUnits[] = {{1e9, "GB"}, {1e6, "MB"}, {1e3, "kB"}};
    for (const Unit& unit : kUnits) {
        const double value = static_cast<double>(bytes) / unit.size;
        if (value >= 1.0) {
            return value < 10.0 ? std::format("{:.1f} {}", value, unit.name)
                                : std::format("{:.0f} {}", value, unit.name);
        }
    }
    return std::format("{} B", bytes);
}

std::string formatSampleRate(uint32_t sampleRateSps) {
    if (sampleRateSps >= 1'000'000) {
        std::string text = std::format("{:.3f}", sampleRateSps / 1e6);
        // Drop trailing zeros: 2.400 → 2.4, 2.048 stays.
        while (text.back() == '0') {
            text.pop_back();
        }
        if (text.back() == '.') {
            text.pop_back();
        }
        return text + " MS/s";
    }
    return std::format("{} kS/s", sampleRateSps / 1000);
}

std::string formatGain(int gainTenthsDb) {
    return std::format("{:.1f} dB", static_cast<double>(gainTenthsDb) / 10.0);
}

double niceStep(double span, int targetTicks) {
    if (span <= 0.0 || targetTicks <= 0) {
        return 1.0;
    }
    const double raw = span / targetTicks;
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    const double normalized = raw / magnitude;
    double step = 10.0;
    if (normalized < 1.5) {
        step = 1.0;
    } else if (normalized < 3.0) {
        step = 2.0;
    } else if (normalized < 7.0) {
        step = 5.0;
    }
    return step * magnitude;
}

int decimalsFor(double step) {
    int decimals = 0;
    while (decimals < 6 && std::fabs(step - std::round(step)) > 1e-9) {
        step *= 10.0;
        ++decimals;
    }
    return decimals;
}

std::string formatSigned(double value, int decimals) {
    std::string text = std::format("{:.{}f}", std::fabs(value), decimals);
    if (value < 0.0 && text.find_first_not_of("0.") != std::string::npos) {
        return std::string(kMinus) + text;
    }
    return text;
}

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

}  // namespace zarescope::ui
