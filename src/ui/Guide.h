// SPDX-License-Identifier: GPL-3.0-or-later
//
// Guide: the user guide's text (resources/help/guide.<language>.md), parsed
// into blocks the Help page draws.
//
// The files are plain Markdown, readable on GitHub as they are, using only:
//
//   # Title                  the page title (once)
//   ## Section               a section: an entry in "On this page"
//   ### Subsection           a heading inside a section (not in the rail)
//   <!-- id: shortcuts -->   (optional, on the line before a section) a
//                            stable name, so the app can jump there
//   paragraph lines          joined until a blank line
//   - item                   a bullet
//   1. step                  a numbered step (the order matters)
//   | label | explanation |  a row of a two-column table; the first row is
//                            the table's header, the |---| line is skipped
//
// Inline marks (**bold**, `code`) are removed: the page sets every block in
// one style, so they would only show as stray characters.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace zarescope::ui {

struct GuideBlock {
    enum class Kind { Title, Section, Subsection, Paragraph, Bullet, Step, TableHeader, Row };
    Kind kind = Kind::Paragraph;
    std::string text;   // the paragraph, item or heading; a row's explanation
    std::string label;  // a row's (or a header's) first column
    int number = 0;     // a step's number
    std::string id;     // a section's stable name, if it has one
};

struct Guide {
    std::vector<GuideBlock> blocks;
    // Index into `blocks` of each Section, in order.
    std::vector<size_t> sections;
};

Guide parseGuide(std::string_view markdown);

}  // namespace zarescope::ui
