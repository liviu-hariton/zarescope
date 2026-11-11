// SPDX-License-Identifier: GPL-3.0-or-later
//
// FrequencyDisplay: the tuned frequency, "137.900 000 MHz", in the top bar
// (design/docs/DESIGN.md §5).
//
// Each digit is its own hit target:
//   scroll over a digit       that decimal place goes up or down
//   click a digit             focuses it (2-px accent underline); then
//     ↑ ↓                     change it
//     ← →                     move to the neighbouring digit
//     0–9                     overwrite it and move right (typing a frequency)
//     Esc, Enter, click away  end editing
// Leading digits are text/primary, the trailing zeros text/secondary.

#pragma once

#include "ui/UiKit.h"

#include <cstdint>
#include <optional>

namespace zarescope::ui {

class FrequencyDisplay {
public:
    // Width of the display for this value.
    [[nodiscard]] float width(const UiKit& ui, int64_t frequencyHz) const;
    // Height of the display (the mono/freq-compact line).
    [[nodiscard]] float height(const UiKit& ui) const;

    // Draws at `pos` and handles input. Returns the new frequency when the
    // user changed it (already clamped to [minHz, maxHz]). When `editable`
    // is false (a recording), it only displays.
    std::optional<int64_t> draw(UiKit& ui, ImVec2 pos, int64_t frequencyHz, bool editable,
                                int64_t minHz, int64_t maxHz);

private:
    int64_t m_focusedPlace = 0;  // place value of the focused digit; 0 = not editing
};

}  // namespace zarescope::ui
