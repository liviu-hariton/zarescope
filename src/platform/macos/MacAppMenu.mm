// SPDX-License-Identifier: GPL-3.0-or-later
//
// AppMenu on macOS: NSApp.mainMenu, built from the MenuSpec data.
//
// App commands go to one target object that queues their numbers; the
// frame loop takes them. Key equivalents are matched by AppKit before the
// window sees the key, so ⌘O, ⌘, and the rest work from anywhere in the app.
// validateMenuItem: reads the per-frame state to enable, check and retitle
// items when a menu opens. Role items use AppKit's own actions with a nil
// target, so they reach the window or the application as usual.
//
// GLFW's default menu is turned off (GLFW_COCOA_MENUBAR, App.cpp) so this
// is the only one.

#include "platform/AppMenu.h"

#import <Cocoa/Cocoa.h>

using zarescope::platform::AppMenu;
using zarescope::platform::MenuItemSpec;
using zarescope::platform::MenuItemState;
using zarescope::platform::MenuSpec;

struct AppMenu::Impl {
    std::vector<int> queue;
    std::map<int, MenuItemState> state;
    id target = nil;
};

@interface ZarescopeMenuTarget : NSObject <NSMenuItemValidation>
@property(nonatomic, assign) AppMenu::Impl* owner;
- (void)perform:(NSMenuItem*)sender;
@end

@implementation ZarescopeMenuTarget

- (void)perform:(NSMenuItem*)sender {
    if (self.owner != nullptr) {
        self.owner->queue.push_back(static_cast<int>(sender.tag));
    }
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
    if (self.owner == nullptr) {
        return NO;
    }
    const auto it = self.owner->state.find(static_cast<int>(item.tag));
    if (it == self.owner->state.end()) {
        return YES;
    }
    item.state = it->second.checked ? NSControlStateValueOn : NSControlStateValueOff;
    if (!it->second.title.empty()) {
        NSString* title = [NSString stringWithUTF8String:it->second.title.c_str()];
        if (title != nil && ![item.title isEqualToString:title]) {
            item.title = title;
        }
    }
    return it->second.enabled ? YES : NO;
}

@end

namespace {

NSString* text(const std::string& value) {
    NSString* s = [NSString stringWithUTF8String:value.c_str()];
    return s != nil ? s : @"";
}

SEL roleAction(const std::string& role) {
    if (role == "hide") return @selector(hide:);
    if (role == "hideOthers") return @selector(hideOtherApplications:);
    if (role == "showAll") return @selector(unhideAllApplications:);
    if (role == "minimize") return @selector(performMiniaturize:);
    if (role == "zoom") return @selector(performZoom:);
    if (role == "bringAllToFront") return @selector(arrangeInFront:);
    if (role == "fullScreen") return @selector(toggleFullScreen:);
    return nil;
}

NSMenu* buildMenu(const std::string& title, const std::vector<MenuItemSpec>& items, id target);

NSMenuItem* buildItem(const MenuItemSpec& spec, id target) {
    if (spec.separator) {
        return [NSMenuItem separatorItem];
    }
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:text(spec.title)
                                                  action:nil
                                           keyEquivalent:text(spec.key)];
    NSEventModifierFlags flags = spec.key.empty() ? 0 : NSEventModifierFlagCommand;
    if (spec.shift) flags |= NSEventModifierFlagShift;
    if (spec.option) flags |= NSEventModifierFlagOption;
    if (spec.control) flags |= NSEventModifierFlagControl;
    item.keyEquivalentModifierMask = flags;
    if (!spec.submenu.empty()) {
        item.submenu = buildMenu(spec.title, spec.submenu, target);
    } else if (!spec.role.empty()) {
        item.action = roleAction(spec.role);
        item.target = nil;  // the responder chain: the window or NSApp
    } else {
        item.action = @selector(perform:);
        item.target = target;
        item.tag = spec.command;
    }
    return item;
}

NSMenu* buildMenu(const std::string& title, const std::vector<MenuItemSpec>& items, id target) {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:text(title)];
    for (const MenuItemSpec& spec : items) {
        [menu addItem:buildItem(spec, target)];
    }
    return menu;
}

}  // namespace

namespace zarescope::platform {

AppMenu::AppMenu() : m_impl(std::make_unique<Impl>()) {
    ZarescopeMenuTarget* target = [[ZarescopeMenuTarget alloc] init];
    target.owner = m_impl.get();
    m_impl->target = target;
}

AppMenu::~AppMenu() {
    static_cast<ZarescopeMenuTarget*>(m_impl->target).owner = nullptr;
    [NSApp setMainMenu:[[NSMenu alloc] init]];
}

void AppMenu::install(const std::vector<MenuSpec>& menus) {
    @autoreleasepool {
        NSMenu* bar = [[NSMenu alloc] initWithTitle:@"Main"];
        for (const MenuSpec& spec : menus) {
            NSMenuItem* holder = [[NSMenuItem alloc] initWithTitle:text(spec.title)
                                                            action:nil
                                                     keyEquivalent:@""];
            NSMenu* menu = buildMenu(spec.title, spec.items, m_impl->target);
            holder.submenu = menu;
            [bar addItem:holder];
            if (spec.role == "window") {
                NSApp.windowsMenu = menu;
            } else if (spec.role == "help") {
                NSApp.helpMenu = menu;
            }
        }
        NSApp.mainMenu = bar;
    }
}

void AppMenu::setState(std::map<int, MenuItemState> state) { m_impl->state = std::move(state); }

std::vector<int> AppMenu::takeCommands() {
    std::vector<int> commands;
    commands.swap(m_impl->queue);
    return commands;
}

void openUrl(const std::string& url) {
    @autoreleasepool {
        NSURL* link = [NSURL URLWithString:text(url)];
        if (link != nil) {
            [[NSWorkspace sharedWorkspace] openURL:link];
        }
    }
}

std::string systemVersion() {
    @autoreleasepool {
        NSOperatingSystemVersion v = [NSProcessInfo processInfo].operatingSystemVersion;
        std::string out = "macOS " + std::to_string(v.majorVersion) + "." +
                          std::to_string(v.minorVersion);
        if (v.patchVersion > 0) {
            out += "." + std::to_string(v.patchVersion);
        }
        return out;
    }
}

}  // namespace zarescope::platform
