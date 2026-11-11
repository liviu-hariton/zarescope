// SPDX-License-Identifier: GPL-3.0-or-later
//
// FileSource: plays a .cu8 recording into the block queue, as if a stick
// were producing it.
//
// The sidecar (IqMetadata) gives the frequency and sample rate. By default
// the file plays at its real rate and loops; tests turn both off to read a
// file as fast as the queue allows, exactly once. Block timestamps continue
// from the recording's start time, so a replay shows the original times.

#pragma once

#include "device/IqMetadata.h"
#include "device/SampleSource.h"

#include <atomic>
#include <filesystem>
#include <thread>

namespace zarescope::device {

struct FileSourceOptions {
    bool realTime = true;  // pace blocks at the recorded sample rate
    bool loop = true;      // start again at the end
};

class FileSource final : public SampleSource {
public:
    // Reads the sidecar. Disk I/O: not on the UI thread. Throws
    // std::runtime_error when the sidecar is missing or invalid.
    FileSource(std::filesystem::path recording, FileSourceOptions options);
    ~FileSource() override;
    FileSource(const FileSource&) = delete;
    FileSource& operator=(const FileSource&) = delete;
    FileSource(FileSource&&) = delete;
    FileSource& operator=(FileSource&&) = delete;

    bool start(BlockQueue& queue, std::string& error) override;
    void stop() override;
    [[nodiscard]] SourceInfo info() const override;

    [[nodiscard]] const IqMetadata& metadata() const { return m_metadata; }
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    void run(BlockQueue& queue);

    std::filesystem::path m_path;
    FileSourceOptions m_options;
    IqMetadata m_metadata;
    std::thread m_thread;
    std::atomic<bool> m_stopRequested{false};
};

}  // namespace zarescope::device
