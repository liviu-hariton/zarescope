// SPDX-License-Identifier: GPL-3.0-or-later
//
// FileDialog on macOS: NSOpenPanel, limited to .cu8 files.

#include "platform/FileDialog.h"

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace zarescope::platform {

std::optional<std::filesystem::path> chooseRecordingToOpen(const std::filesystem::path& folder) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        // .cu8 has no system type; describe it by its extension.
        UTType* cu8 = [UTType typeWithFilenameExtension:@"cu8"];
        if (cu8 != nil) {
            panel.allowedContentTypes = @[ cu8 ];
        }
        std::error_code ec;
        if (!folder.empty() && std::filesystem::is_directory(folder, ec)) {
            panel.directoryURL =
                [NSURL fileURLWithPath:[NSString stringWithUTF8String:folder.c_str()]
                           isDirectory:YES];
        }
        if ([panel runModal] != NSModalResponseOK || panel.URLs.count == 0) {
            return std::nullopt;
        }
        return std::filesystem::path(panel.URLs.firstObject.fileSystemRepresentation);
    }
}

}  // namespace zarescope::platform
