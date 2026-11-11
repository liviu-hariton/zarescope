// SPDX-License-Identifier: GPL-3.0-or-later
//
// FileDialog: the operating system's own "open file" dialog.

#pragma once

#include <filesystem>
#include <optional>

namespace zarescope::platform {

// Asks the user for a .cu8 recording, starting in `folder` when it exists.
// Returns std::nullopt when the user cancels. Modal: call from the UI
// thread; the frame loop pauses while the dialog is open (the receive
// threads keep running).
std::optional<std::filesystem::path> chooseRecordingToOpen(const std::filesystem::path& folder);

}  // namespace zarescope::platform
