// SPDX-License-Identifier: GPL-3.0-or-later
//
// FileDialog: the operating system's own open, save and folder dialogs.

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace zarescope::platform {

// Asks the user for a .cu8 recording, starting in `folder` when it exists.
// Returns std::nullopt when the user cancels. Modal: call from the UI
// thread; the frame loop pauses while the dialog is open (the receive
// threads keep running).
std::optional<std::filesystem::path> chooseRecordingToOpen(const std::filesystem::path& folder);

// "Save as" for a journal export, suggesting `defaultName` (its extension
// decides the type). Modal, UI thread, like the one above.
std::optional<std::filesystem::path> chooseExportFile(const std::string& defaultName);

// Picks a folder (the data folder), starting in `start` when it exists; the
// dialog can create a new one.
std::optional<std::filesystem::path> chooseFolder(const std::filesystem::path& start);

}  // namespace zarescope::platform
