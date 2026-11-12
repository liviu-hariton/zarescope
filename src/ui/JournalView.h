// SPDX-License-Identifier: GPL-3.0-or-later
//
// JournalView: the Journal tab (M6).
//
//   filter bar   kind (All, Passes, Images, Aircraft, Recordings), a search
//                field, and Export CSV / Export JSON on the right
//   table        newest first: local time, kind, event, details, and a
//                Show button for events with a file or folder
//   empty state  what will appear here, and when
//
// It draws the JournalService snapshot and returns what the user asked
// for; the queries and exports run on the journal's thread.

#pragma once

#include "store/Journal.h"
#include "ui/UiKit.h"

#include <array>
#include <filesystem>
#include <optional>

namespace zarescope::ui {

struct JournalViewActions {
    std::optional<store::JournalFilter> filter;  // changed: query again
    std::optional<bool> exportJson;              // Export clicked: true = JSON, false = CSV
    std::optional<std::filesystem::path> reveal;  // Show clicked
};

class JournalView {
public:
    JournalViewActions draw(UiKit& ui, ImVec2 pos, ImVec2 size,
                            const store::JournalService::Snapshot& snapshot);

private:
    [[nodiscard]] store::JournalFilter filter() const;

    int m_kind = 0;  // 0 = all, then the EventKinds in order
    std::array<char, 128> m_search{};
};

}  // namespace zarescope::ui
