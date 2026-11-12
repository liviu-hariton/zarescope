// SPDX-License-Identifier: GPL-3.0-or-later
//
// FileDialog on macOS: NSOpenPanel (recordings, folders) and NSSavePanel (exports).

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

std::optional<std::filesystem::path> chooseExportFile(const std::string& defaultName) {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        NSString* name = [NSString stringWithUTF8String:defaultName.c_str()];
        if (name != nil) {
            panel.nameFieldStringValue = name;
            UTType* type = [UTType typeWithFilenameExtension:name.pathExtension];
            if (type != nil) {
                panel.allowedContentTypes = @[ type ];
            }
        }
        panel.canCreateDirectories = YES;
        if ([panel runModal] != NSModalResponseOK || panel.URL == nil) {
            return std::nullopt;
        }
        return std::filesystem::path(panel.URL.fileSystemRepresentation);
    }
}

std::optional<std::filesystem::path> chooseFolder(const std::filesystem::path& start) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = NO;
        panel.canChooseDirectories = YES;
        panel.canCreateDirectories = YES;
        panel.allowsMultipleSelection = NO;
        std::error_code ec;
        if (!start.empty() && std::filesystem::is_directory(start, ec)) {
            panel.directoryURL =
                [NSURL fileURLWithPath:[NSString stringWithUTF8String:start.c_str()]
                           isDirectory:YES];
        }
        if ([panel runModal] != NSModalResponseOK || panel.URLs.count == 0) {
            return std::nullopt;
        }
        return std::filesystem::path(panel.URLs.firstObject.fileSystemRepresentation);
    }
}

}  // namespace zarescope::platform
