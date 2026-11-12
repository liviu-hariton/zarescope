// SPDX-License-Identifier: GPL-3.0-or-later
//
// AppMenuModel: what the menu bar contains (DESIGN.md §5, "Menu bar"), as
// plain data the platform turns into its native menus.
//
//   Zarescope  About, Settings… ⌘,, Hide, Hide Others, Show All, Quit ⌘Q
//   File       Connect Receiver… ⇧⌘K, Open Recording… ⌘O,
//              Start/Stop Recording ⇧⌘R, Close Receiver/Recording,
//              Export Journal as CSV… / JSON…, Close Window ⌘W
//   Edit       Undo, Redo, Cut, Copy, Paste, Select All (for text fields)
//   View       the tabs ⌘1–⌘6, Bigger / Smaller / Standard Text ⌘+ ⌘- ⌘0,
//              Dark / Light Theme, Enter Full Screen ⌃⌘F
//   Receiver   Tune To › the presets, Record and Decode Passes
//              Automatically, Look for SatDump Again
//   Window     Minimize ⌘M, Zoom, Bring All to Front
//   Help       Zarescope Help ⌘?, Keyboard Shortcuts, the website, the
//              source code, Report a Problem…
//
// Titles are Title Case in English, as macOS menus are, and sentence case
// in Romanian. MainWindow decides each frame which commands are enabled.

#pragma once

#include "platform/AppMenu.h"
#include "ui/Strings.h"

#include <string>
#include <vector>

namespace zarescope::ui {

enum class Command : int {
    None = 0,
    About,
    Settings,
    Quit,
    ConnectReceiver,
    OpenRecording,
    ToggleRecording,
    CloseSource,
    ExportCsv,
    ExportJson,
    CloseWindow,
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    SelectAll,
    ShowDiscover,
    ShowSpectrum,
    ShowAircraft,
    ShowSatellites,
    ShowJournal,
    ShowHelp,
    BiggerText,
    SmallerText,
    StandardText,
    DarkTheme,
    LightTheme,
    ToggleAutoRecord,
    CheckSatDump,
    HelpShortcuts,
    Website,
    SourceCode,
    CheckUpdates,
    ReportProblem,
    PresetFirst = 1000,  // + the preset's index in presets.json
};

// The project's addresses, opened from the Help menu and the About window.
inline constexpr const char* kWebsiteUrl = "https://zarescope.eu";
inline constexpr const char* kSourceUrl = "https://github.com/liviu-hariton/zarescope";
inline constexpr const char* kIssuesUrl = "https://github.com/liviu-hariton/zarescope/issues";

// `presetTitles`: the presets' display names, in presets.json order.
std::vector<platform::MenuSpec> buildMenus(Language language,
                                           const std::vector<std::string>& presetTitles);

// The menu bar as indented text, for --print-menus and the tests.
std::string describeMenus(const std::vector<platform::MenuSpec>& menus);

}  // namespace zarescope::ui
