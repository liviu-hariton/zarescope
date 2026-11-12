// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/AppMenuModel.h"

namespace zarescope::ui {

namespace {

using platform::MenuItemSpec;
using platform::MenuSpec;

MenuItemSpec separator() {
    MenuItemSpec item;
    item.separator = true;
    return item;
}

}  // namespace

std::vector<MenuSpec> buildMenus(Language language, const std::vector<std::string>& presetTitles) {
    auto t = [language](StringId id) { return std::string(text(id, language)); };
    auto command = [&](StringId id, Command cmd, std::string key = {}, bool shift = false) {
        MenuItemSpec item;
        item.title = t(id);
        item.command = static_cast<int>(cmd);
        item.key = std::move(key);
        item.shift = shift;
        return item;
    };
    auto role = [&](StringId id, std::string name, std::string key = {}, bool option = false) {
        MenuItemSpec item;
        item.title = t(id);
        item.role = std::move(name);
        item.key = std::move(key);
        item.option = option;
        return item;
    };

    std::vector<MenuSpec> menus;

    menus.push_back({"Zarescope",
                     "app",
                     {command(StringId::MenuAbout, Command::About),
                      command(StringId::MenuCheckUpdates, Command::CheckUpdates), separator(),
                      command(StringId::MenuSettings, Command::Settings, ","), separator(),
                      role(StringId::MenuHide, "hide", "h"),
                      role(StringId::MenuHideOthers, "hideOthers", "h", true),
                      role(StringId::MenuShowAll, "showAll"), separator(),
                      command(StringId::MenuQuit, Command::Quit, "q")}});

    menus.push_back({t(StringId::MenuFile),
                     "",
                     {command(StringId::MenuConnect, Command::ConnectReceiver, "k", true),
                      command(StringId::MenuOpenRecording, Command::OpenRecording, "o"), separator(),
                      command(StringId::MenuStartRecording, Command::ToggleRecording, "r", true),
                      command(StringId::MenuCloseReceiver, Command::CloseSource), separator(),
                      command(StringId::MenuExportCsv, Command::ExportCsv),
                      command(StringId::MenuExportJson, Command::ExportJson), separator(),
                      command(StringId::MenuCloseWindow, Command::CloseWindow, "w")}});

    menus.push_back({t(StringId::MenuEdit),
                     "",
                     {command(StringId::MenuUndo, Command::Undo, "z"),
                      command(StringId::MenuRedo, Command::Redo, "z", true), separator(),
                      command(StringId::MenuCut, Command::Cut, "x"),
                      command(StringId::MenuCopy, Command::Copy, "c"),
                      command(StringId::MenuPaste, Command::Paste, "v"),
                      command(StringId::MenuSelectAll, Command::SelectAll, "a")}});

    MenuItemSpec fullScreen = role(StringId::MenuFullScreen, "fullScreen", "f");
    fullScreen.control = true;
    menus.push_back({t(StringId::MenuView),
                     "",
                     {command(StringId::TabDiscover, Command::ShowDiscover, "1"),
                      command(StringId::TabSpectrum, Command::ShowSpectrum, "2"),
                      command(StringId::TabAircraft, Command::ShowAircraft, "3"),
                      command(StringId::TabSatellites, Command::ShowSatellites, "4"),
                      command(StringId::TabJournal, Command::ShowJournal, "5"),
                      command(StringId::TabHelp, Command::ShowHelp, "6"), separator(),
                      command(StringId::MenuBiggerText, Command::BiggerText, "+"),
                      command(StringId::MenuSmallerText, Command::SmallerText, "-"),
                      command(StringId::MenuActualText, Command::StandardText, "0"), separator(),
                      command(StringId::MenuDarkTheme, Command::DarkTheme),
                      command(StringId::MenuLightTheme, Command::LightTheme), separator(),
                      fullScreen}});

    MenuItemSpec tuneTo;
    tuneTo.title = t(StringId::MenuTuneTo);
    for (size_t i = 0; i < presetTitles.size(); ++i) {
        MenuItemSpec preset;
        preset.title = presetTitles[i];
        preset.command = static_cast<int>(Command::PresetFirst) + static_cast<int>(i);
        tuneTo.submenu.push_back(preset);
    }
    std::vector<MenuItemSpec> receiver;
    if (!tuneTo.submenu.empty()) {
        receiver.push_back(tuneTo);
        receiver.push_back(separator());
    }
    receiver.push_back(command(StringId::MenuAutoRecord, Command::ToggleAutoRecord));
    receiver.push_back(command(StringId::MenuCheckSatDump, Command::CheckSatDump));
    menus.push_back({t(StringId::MenuReceiver), "", receiver});

    menus.push_back({t(StringId::MenuWindow),
                     "window",
                     {role(StringId::MenuMinimize, "minimize", "m"), role(StringId::MenuZoom, "zoom"),
                      separator(), role(StringId::MenuBringAll, "bringAllToFront")}});

    menus.push_back({t(StringId::MenuHelp),
                     "help",
                     {command(StringId::MenuGuide, Command::ShowHelp, "?"),
                      command(StringId::MenuShortcuts, Command::HelpShortcuts), separator(),
                      command(StringId::MenuWebsite, Command::Website),
                      command(StringId::MenuSourceCode, Command::SourceCode),
                      command(StringId::MenuReportProblem, Command::ReportProblem)}});
    return menus;
}

namespace {

void describeItems(const std::vector<MenuItemSpec>& items, int depth, std::string& out) {
    for (const MenuItemSpec& item : items) {
        out.append(static_cast<size_t>(depth) * 2, ' ');
        if (item.separator) {
            out += "---\n";
            continue;
        }
        out += item.title;
        if (!item.key.empty()) {
            out += "  [";
            out += item.control ? "⌃" : "";
            out += item.option ? "⌥" : "";
            out += item.shift ? "⇧" : "";
            out += "⌘" + item.key + "]";
        }
        out += "\n";
        describeItems(item.submenu, depth + 1, out);
    }
}

}  // namespace

std::string describeMenus(const std::vector<MenuSpec>& menus) {
    std::string out;
    for (const MenuSpec& menu : menus) {
        out += menu.title + "\n";
        describeItems(menu.items, 1, out);
    }
    return out;
}

}  // namespace zarescope::ui
