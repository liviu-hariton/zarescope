// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/theme/PlotTheme.h"

#include "ui/theme/ImGuiTheme.h"

#include <array>
#include <string_view>

namespace zarescope::ui {

namespace {

struct PlotColor {
    ImPlotCol slot;
    std::string_view name;
    std::string_view expression;  // a token, or "transparent"
};

constexpr std::array kPlotColors{
    PlotColor{ImPlotCol_FrameBg, "FrameBg", "transparent"},
    PlotColor{ImPlotCol_PlotBg, "PlotBg", "bg/base"},
    PlotColor{ImPlotCol_PlotBorder, "PlotBorder", "border/subtle"},
    PlotColor{ImPlotCol_LegendBg, "LegendBg", "bg/raised"},
    PlotColor{ImPlotCol_LegendBorder, "LegendBorder", "border/subtle"},
    PlotColor{ImPlotCol_LegendText, "LegendText", "text/primary"},
    PlotColor{ImPlotCol_TitleText, "TitleText", "text/primary"},
    PlotColor{ImPlotCol_InlayText, "InlayText", "text/secondary"},
    PlotColor{ImPlotCol_AxisText, "AxisText", "text/secondary"},
    PlotColor{ImPlotCol_AxisGrid, "AxisGrid", "border/subtle"},
    PlotColor{ImPlotCol_AxisTick, "AxisTick", "border/strong"},
    PlotColor{ImPlotCol_AxisBg, "AxisBg", "transparent"},
    PlotColor{ImPlotCol_AxisBgHovered, "AxisBgHovered", "bg/hover"},
    PlotColor{ImPlotCol_AxisBgActive, "AxisBgActive", "bg/hover"},
    PlotColor{ImPlotCol_Selection, "Selection", "accent/soft"},
    PlotColor{ImPlotCol_Crosshairs, "Crosshairs", "text/secondary"},
};
static_assert(kPlotColors.size() == ImPlotCol_COUNT, "every ImPlotCol needs a token");

}  // namespace

std::vector<std::string> applyPlotTheme(const theme::DesignTokens& tokens,
                                        theme::ThemeVariant variant, ImPlotStyle& style) {
    std::vector<std::string> missing;
    for (const PlotColor& color : kPlotColors) {
        if (auto rgba = tokens.resolveColor(variant, color.expression)) {
            style.Colors[color.slot] = toImVec4(*rgba);
        } else {
            missing.emplace_back(color.name);
        }
    }
    const float border = tokens.border("default").value_or(1.0F);
    const float small = tokens.spacing("4").value_or(4.0F);
    style.PlotBorderSize = border;
    style.MinorAlpha = 0.0F;  // major grid lines only: a calm plot
    style.MajorGridSize = ImVec2(border, border);
    style.MajorTickSize = ImVec2(border, border);
    style.PlotPadding = ImVec2(0.0F, 0.0F);
    style.LabelPadding = ImVec2(small, small);
    style.MajorTickLen = ImVec2(small, small);
    style.MinorTickLen = ImVec2(0.0F, 0.0F);
    return missing;
}

}  // namespace zarescope::ui
