// SPDX-License-Identifier: GPL-3.0-or-later
//
// Desktop on macOS: NSWorkspace for Finder, NSProcessInfo activities against
// App Nap and idle sleep (Apple's "Prioritize Work at the App Level" guide).

#include "platform/Desktop.h"

#import <Cocoa/Cocoa.h>

namespace zarescope::platform {

void revealInFileManager(const std::filesystem::path& path) {
    @autoreleasepool {
        NSString* text = [NSString stringWithUTF8String:path.c_str()];
        if (text == nil) {
            return;
        }
        NSURL* url = [NSURL fileURLWithPath:text];
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
    }
}

struct KeepAwake::Impl {
    id<NSObject> activity = nil;  // the current NSProcessInfo activity, if any
};

KeepAwake::KeepAwake() : m_impl(std::make_unique<Impl>()) {}

KeepAwake::~KeepAwake() { set(Level::Normal); }

void KeepAwake::set(Level level) {
    if (level == m_level) {
        return;
    }
    m_level = level;
    NSProcessInfo* process = [NSProcessInfo processInfo];
    if (m_impl->activity != nil) {
        [process endActivity:m_impl->activity];
        m_impl->activity = nil;
    }
    switch (level) {
        case Level::Normal: break;
        case Level::Waiting:
            m_impl->activity =
                [process beginActivityWithOptions:NSActivityUserInitiatedAllowingIdleSystemSleep
                                           reason:@"Waiting for the next satellite pass"];
            break;
        case Level::Pass:
            m_impl->activity = [process beginActivityWithOptions:NSActivityUserInitiated
                                                          reason:@"Recording a satellite pass"];
            break;
    }
}

}  // namespace zarescope::platform
