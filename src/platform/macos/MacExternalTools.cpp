// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where SatDump lives on macOS. The Homebrew cask (`brew install --cask
// satdump`) puts SatDump.app in /Applications and links
// /opt/homebrew/bin/satdump to the binary inside it. The binary finds its
// pipelines and plugins relative to the path it was started by, so it must
// be run by its real path inside the bundle, not by the link: the caller
// resolves links (std::filesystem::canonical) before running it.

#include "platform/Process.h"

#include <cstdlib>
#include <string_view>

namespace zarescope::platform {

std::vector<std::filesystem::path> satDumpCandidates() {
    const std::filesystem::path inBundle =
        std::filesystem::path("SatDump.app") / "Contents" / "MacOS" / "satdump";
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(std::filesystem::path("/Applications") / inBundle);
    if (const char* home = std::getenv("HOME")) {
        candidates.push_back(std::filesystem::path(home) / "Applications" / inBundle);
    }
    // An app started from Finder gets a minimal PATH; Homebrew's own bin
    // directories are listed explicitly so they are found anyway.
    candidates.emplace_back("/opt/homebrew/bin/satdump");
    candidates.emplace_back("/usr/local/bin/satdump");
    if (const char* path = std::getenv("PATH")) {
        std::string_view rest(path);
        while (!rest.empty()) {
            const size_t colon = rest.find(':');
            const std::string_view dir = rest.substr(0, colon);
            if (!dir.empty()) {
                candidates.push_back(std::filesystem::path(dir) / "satdump");
            }
            rest = colon == std::string_view::npos ? std::string_view{} : rest.substr(colon + 1);
        }
    }
    return candidates;
}

}  // namespace zarescope::platform
