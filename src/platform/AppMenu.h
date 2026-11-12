// SPDX-License-Identifier: GPL-3.0-or-later
//
// AppMenu: the operating system's menu bar (on macOS, the native NSMenu at
// the top of the screen).
//
// The app describes its menus as plain data (MenuSpec, built by
// ui::buildMenus) and gets back the numbers of the commands the user chose.
// The platform code knows nothing about what the commands do.
//
//   install()       builds the menu bar (again, when the language changes)
//   setState()      each frame: which commands are enabled or checked, and
//                   titles that change ("Start Recording" / "Stop Recording").
//                   The OS asks for these when a menu opens, so items are
//                   disabled, not hidden, when they can't run.
//   takeCommands()  each frame: what the user chose since the last call
//                   (from a click or a keyboard shortcut), in order
//
// Standard items (Hide, Minimize, Zoom, Full Screen…) are named by `role`
// and handled by the OS itself. UI thread only.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace zarescope::platform {

struct MenuItemSpec {
    std::string title;
    int command = 0;  // 0: no app command (a role, a separator or a submenu)
    // Keyboard shortcut: a character ("o", ",", "1", "?"), with ⌘ implied,
    // plus these modifiers. Empty: none.
    std::string key;
    bool shift = false;
    bool option = false;
    bool control = false;
    // An item the OS handles: "hide", "hideOthers", "showAll", "minimize",
    // "zoom", "bringAllToFront", "fullScreen". Empty: an app command.
    std::string role;
    bool separator = false;
    std::vector<MenuItemSpec> submenu;
};

struct MenuSpec {
    std::string title;
    // "app" (the first menu, named after the app), "window" (the OS lists
    // the open windows there) or "help" (the OS adds its search field).
    std::string role;
    std::vector<MenuItemSpec> items;
};

struct MenuItemState {
    bool enabled = true;
    bool checked = false;
    std::string title;  // empty: keep the installed title
};

class AppMenu {
public:
    AppMenu();
    ~AppMenu();
    AppMenu(const AppMenu&) = delete;
    AppMenu& operator=(const AppMenu&) = delete;
    AppMenu(AppMenu&&) = delete;
    AppMenu& operator=(AppMenu&&) = delete;

    void install(const std::vector<MenuSpec>& menus);
    void setState(std::map<int, MenuItemState> state);
    std::vector<int> takeCommands();

    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
};

// Opens a web page in the default browser.
void openUrl(const std::string& url);

// "macOS 15.1": the OS and its version, for the About window.
std::string systemVersion();

}  // namespace zarescope::platform
