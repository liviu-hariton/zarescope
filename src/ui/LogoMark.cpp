// SPDX-License-Identifier: GPL-3.0-or-later
//
// The logo symbol. Coordinates below are the SVG files' own (viewBox units);
// `toScreen` maps them onto the requested square.

#include "ui/LogoMark.h"

#include <cmath>

namespace zarescope::ui {

namespace {

// Below this size the brand rules switch to the 16-grid drawing.
constexpr float kSmallSymbolBelowPx = 40.0F;

struct Mapping {
    ImVec2 origin;
    float scale;  // screen px per viewBox unit

    [[nodiscard]] ImVec2 toScreen(float x, float y) const {
        return {origin.x + x * scale, origin.y + y * scale};
    }
};

ImVec2 cubicPoint(ImVec2 p0, ImVec2 p1, ImVec2 p2, ImVec2 p3, float t) {
    const float u = 1.0F - t;
    const float a = u * u * u;
    const float b = 3.0F * u * u * t;
    const float c = 3.0F * u * t * t;
    const float d = t * t * t;
    return {a * p0.x + b * p1.x + c * p2.x + d * p3.x, a * p0.y + b * p1.y + c * p2.y + d * p3.y};
}

// A dotted cubic curve: round dots of `diameter` every `period` along the
// curve, like the SVG's `stroke-dasharray="0.1 6.4"` with round caps.
void dottedCubic(ImDrawList& drawList, ImVec2 p0, ImVec2 p1, ImVec2 p2, ImVec2 p3, float period,
                 float diameter, ImU32 color) {
    constexpr int kSteps = 256;
    float travelled = 0.0F;
    float nextDot = 0.0F;
    ImVec2 previous = p0;
    for (int i = 0; i <= kSteps; ++i) {
        const ImVec2 point = cubicPoint(p0, p1, p2, p3, static_cast<float>(i) / kSteps);
        travelled += std::hypot(point.x - previous.x, point.y - previous.y);
        previous = point;
        if (travelled >= nextDot) {
            drawList.AddCircleFilled(point, diameter * 0.5F, color);
            nextDot += period;
        }
    }
}

// symbol-small-*.svg (viewBox 0 0 16 16).
void drawSmall(ImDrawList& drawList, const Mapping& m, const LogoColors& colors) {
    const float s = m.scale;
    drawList.AddCircle(m.toScreen(8.0F, 8.0F), 6.5F * s, colors.rings, 0, 1.5F * s);
    drawList.AddBezierCubic(m.toScreen(2.6F, 11.2F), m.toScreen(5.0F, 6.5F), m.toScreen(9.5F, 4.0F),
                            m.toScreen(14.3F, 5.3F), colors.pass, 1.5F * s);
    // Knockout gap, then the satellite.
    drawList.AddCircleFilled(m.toScreen(8.8F, 5.46F), 2.9F * s, colors.background);
    drawList.AddCircleFilled(m.toScreen(8.8F, 5.46F), 1.9F * s, colors.pass);
}

// symbol-*.svg (viewBox 0 0 64 64).
void drawFull(ImDrawList& drawList, const Mapping& m, const LogoColors& colors) {
    const float s = m.scale;
    // Horizon (0°) and 45° elevation rings, and the north tick.
    drawList.AddCircle(m.toScreen(32.0F, 32.0F), 26.0F * s, colors.rings, 0, 3.5F * s);
    drawList.AddCircle(m.toScreen(32.0F, 32.0F), 13.0F * s, colors.rings, 0, 2.5F * s);
    drawList.AddLine(m.toScreen(32.0F, 6.0F), m.toScreen(32.0F, 13.0F), colors.rings, 3.5F * s);
    // Flown part of the pass: solid.
    drawList.AddBezierCubic(m.toScreen(10.5F, 46.6F), m.toScreen(16.2F, 34.24F),
                            m.toScreen(24.96F, 25.7F), m.toScreen(34.84F, 21.92F), colors.pass,
                            3.5F * s);
    // Predicted part: dotted.
    dottedCubic(drawList, m.toScreen(34.84F, 21.92F), m.toScreen(41.42F, 19.4F),
                m.toScreen(48.5F, 19.0F), m.toScreen(55.5F, 21.0F), 6.5F * s, 3.5F * s,
                colors.pass);
    // Knockout gap, then the satellite.
    drawList.AddCircleFilled(m.toScreen(34.84F, 21.92F), 7.0F * s, colors.background);
    drawList.AddCircleFilled(m.toScreen(34.84F, 21.92F), 5.5F * s, colors.pass);
}

}  // namespace

void drawLogoSymbol(ImDrawList& drawList, ImVec2 topLeft, float sizePx, const LogoColors& colors) {
    if (sizePx < kSmallSymbolBelowPx) {
        drawSmall(drawList, Mapping{topLeft, sizePx / 16.0F}, colors);
    } else {
        drawFull(drawList, Mapping{topLeft, sizePx / 64.0F}, colors);
    }
}

}  // namespace zarescope::ui
