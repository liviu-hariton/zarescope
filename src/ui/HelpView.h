// SPDX-License-Identifier: GPL-3.0-or-later
//
// HelpView: the Help tab, the user guide inside the app.
//
//   On this page   a rail on the left, fixed while the text scrolls: one
//                  entry per section, the section being read marked;
//                  clicking an entry scrolls there
//   the text       one reading column (about 75 characters a line):
//                  the title, sections with a heading, paragraphs, bullets,
//                  numbered steps, and label/explanation rows with hairlines
//                  between them; the version and the website close the page
//
// It scrolls with the wheel or trackpad, and from the keyboard (arrows,
// Page Up/Down, Home/End) when no text field is being typed in. The layout
// is measured once per guide and width, and only the visible blocks are
// drawn each frame.

#pragma once

#include "ui/Guide.h"
#include "ui/UiKit.h"

#include <optional>
#include <string>
#include <vector>

namespace zarescope::ui {

class HelpView {
public:
    struct Actions {
        bool openWebsite = false;
    };

    // `guide` is null when the guide file couldn't be read. `footer` is the
    // version line under the last section.
    Actions draw(UiKit& ui, ImVec2 pos, ImVec2 size, const Guide* guide, std::string_view footer);

    // Scrolls to the section named `id` (<!-- id: … --> in the file) on the
    // next draw; the top of the page for an empty id.
    void jumpTo(std::string id) { m_jumpId = std::move(id); }

private:
    void layout(UiKit& ui, const Guide& guide, float width);

    std::vector<float> m_blockY;  // each block's top, from the page's top
    std::vector<float> m_blockH;
    float m_contentHeight = 0.0F;
    const Guide* m_laidOut = nullptr;
    float m_laidOutWidth = 0.0F;
    Language m_laidOutLanguage = Language::English;
    std::optional<std::string> m_jumpId;
    std::optional<float> m_scrollTo;
};

}  // namespace zarescope::ui
