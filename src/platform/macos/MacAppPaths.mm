// SPDX-License-Identifier: GPL-3.0-or-later
//
// AppPaths on macOS: the bundle's Resources folder and
// ~/Library/Application Support/Zarescope.

#include "platform/AppPaths.h"

#import <Foundation/Foundation.h>

namespace zarescope::platform {

AppPaths appPaths() {
    AppPaths paths;
    @autoreleasepool {
        // For Zarescope.app this is Contents/Resources. A bare executable
        // outside a bundle gets its own folder instead.
        NSString* resources = [[NSBundle mainBundle] resourcePath];
        if (resources != nil) {
            paths.resources = std::filesystem::path(resources.fileSystemRepresentation);
        }

        NSArray<NSURL*>* support =
            [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory
                                                   inDomains:NSUserDomainMask];
        if (support.count > 0) {
            paths.userData =
                std::filesystem::path(support.firstObject.fileSystemRepresentation) / "Zarescope";
        }
    }
    return paths;
}

}  // namespace zarescope::platform
