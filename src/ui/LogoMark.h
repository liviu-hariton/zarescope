// SPDX-License-Identifier: GPL-3.0-or-later
//
// The "Polar Sky" logo symbol, drawn with ImGui's vector primitives.
//
// The geometry is copied from design/brand/logo/symbol-*.svg, so the app
// draws the logo sharp at any size without an SVG rasterizer. Two drawings
// exist (design/docs/DESIGN.md §1):
//   Small  the 16-grid symbol for sizes below 40 px: one ring, a solid track
//   Full   the 64-grid symbol from 40 px up: horizon and 45° rings, the north
//          tick, the flown (solid) and predicted (dotted) track
// Both put the satellite dot on the track with a knockout gap around it.

#pragma once

#include <imgui.h>

namespace zarescope::ui {

struct LogoColors {
    ImU32 rings = 0;       // the logo's ring colour token
    ImU32 pass = 0;        // the logo's pass colour token
    ImU32 background = 0;  // the surface the logo sits on (fills the knockout gap)
};

// Draws the symbol in the square at `topLeft` with side `sizePx`. Picks the
// small or full drawing by size, as the brand rules require.
void drawLogoSymbol(ImDrawList& drawList, ImVec2 topLeft, float sizePx, const LogoColors& colors);

}  // namespace zarescope::ui
