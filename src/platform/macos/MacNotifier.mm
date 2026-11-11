// SPDX-License-Identifier: GPL-3.0-or-later
//
// Notifier on macOS: UNUserNotificationCenter. Needs the app bundle (its
// identifier); a bare executable outside Zarescope.app gets no center and
// notifications are skipped.

#include "platform/Notifier.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

namespace zarescope::platform {

void notify(const std::string& title, const std::string& body) {
    @autoreleasepool {
        if ([[NSBundle mainBundle] bundleIdentifier] == nil) {
            return;  // not running from the bundle
        }
        UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
        NSString* titleText = [NSString stringWithUTF8String:title.c_str()];
        NSString* bodyText = [NSString stringWithUTF8String:body.c_str()];
        [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert |
                                                 UNAuthorizationOptionSound)
                              completionHandler:^(BOOL granted, NSError* _Nullable) {
                                if (!granted) {
                                    return;
                                }
                                UNMutableNotificationContent* content =
                                    [UNMutableNotificationContent new];
                                content.title = titleText;
                                content.body = bodyText;
                                content.sound = [UNNotificationSound defaultSound];
                                UNNotificationRequest* request = [UNNotificationRequest
                                    requestWithIdentifier:[[NSUUID UUID] UUIDString]
                                                  content:content
                                                  trigger:nil];
                                [center addNotificationRequest:request withCompletionHandler:nil];
                              }];
    }
}

}  // namespace zarescope::platform
