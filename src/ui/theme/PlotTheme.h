// SPDX-License-Identifier: GPL-3.0-or-later
//
// The design tokens applied to ImPlot (the spectrum plot).
//
// The token file has no ImPlot section, so the mapping lives here, by token
// name, like the ImGuiCol fallbacks in ImGuiTheme.cpp: the plot sits on the
// window background, grid and border use the subtle border colour, axis text
// the secondary text colour. The trace's own colours (accent line, accent/soft
// fill) are set where it is drawn.

#pragma once

#include "ui/theme/DesignTokens.h"

#include <implot.h>

#include <string>
#include <vector>

namespace zarescope::ui {

// Returns the ImPlotCol names it could not give a colour (empty = complete).
std::vector<std::string> applyPlotTheme(const theme::DesignTokens& tokens,
                                        theme::ThemeVariant variant, ImPlotStyle& style);

}  // namespace zarescope::ui
