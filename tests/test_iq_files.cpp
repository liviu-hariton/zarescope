// SPDX-License-Identifier: GPL-3.0-or-later
//
// IQ recordings: the sidecar format, FileSource playback and IqRecorder.
// Includes M1's acceptance check that a recorded file replays identically.

#include "device/FileSource.h"
#include "device/IqMetadata.h"
#include "device/IqRecorder.h"
#include "dsp/SignalGenerator.h"

#include <doctest/doctest.h>

#include <array>
#include <fstream>
#include <random>
#include <thread>

using namespace zarescope::device;
using namespace std::chrono;

namespace {

// A fresh folder under the system temp directory, removed afterwards.
struct TempDir {
    std::filesystem::path path;
    TempDir() {
        std::random_device random;
        path = std::filesystem::temp_directory_path() /
               ("zarescope-test-" + std::to_string(random()) + std::to_string(random()));
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;
};

IqMetadata sampleMetadata() {
    IqMetadata metadata;
    metadata.centerFrequencyHz = 100'000'000;
    metadata.sampleRateSps = 1'024'000;
    metadata.startUtc = sys_days{year{2026} / November / 11} + hours{21} + minutes{7} + seconds{5} +
                        milliseconds{750};
    metadata.gainTenthsDb = 297;
    metadata.ppm = -2;
    metadata.device = "synthetic";
    metadata.software = "Zarescope test";
    return metadata;
}

std::vector<uint8_t> syntheticBytes(size_t bytes, uint32_t rate) {
    zarescope::dsp::SignalGenerator generator(rate, 0.05F, 3);
    const std::array carriers{zarescope::dsp::Carrier{.offsetHz = 50'000.0, .amplitude = 0.4F}};
    std::vector<uint8_t> iq(bytes);
    generator.generate(carriers, iq);
    return iq;
}

void writeRecording(const std::filesystem::path& path, const std::vector<uint8_t>& bytes,
                    const IqMetadata& metadata) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    std::string error;
    REQUIRE(writeSidecar(path, metadata, error));
}

std::vector<uint8_t> readAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Plays a recording once, as fast as possible, and returns every block.
std::vector<SampleBlock> playOnce(const std::filesystem::path& path) {
    FileSource source(path, {.realTime = false, .loop = false});
    BlockQueue queue = makeBlockQueue(8);
    std::string error;
    REQUIRE(source.start(queue, error));
    std::vector<SampleBlock> blocks;
    for (;;) {
        if (SampleBlock* block = queue.beginRead()) {
            blocks.push_back(*block);
            queue.commitRead();
        } else if (source.finished()) {
            if (queue.beginRead() == nullptr) {
                break;
            }
        } else {
            std::this_thread::yield();
        }
    }
    source.stop();
    CHECK(source.lastError().empty());
    CHECK(source.droppedBlocks() == 0);
    return blocks;
}

}  // namespace

TEST_CASE("sidecar: UTC timestamps round-trip at millisecond precision") {
    const auto when = sampleMetadata().startUtc;
    CHECK(formatUtcTimestamp(when) == "2026-11-11T21:07:05.750Z");
    CHECK(parseUtcTimestamp("2026-11-11T21:07:05.750Z") == when);
    CHECK(parseUtcTimestamp("2026-11-11T21:07:05Z") == when - milliseconds{750});
    CHECK(parseUtcTimestamp("2026-11-11T21:07:05.75Z") == when);
    CHECK_FALSE(parseUtcTimestamp("2026-11-11 21:07:05Z"));
    CHECK_FALSE(parseUtcTimestamp("2026-02-30T21:07:05Z"));
    CHECK_FALSE(parseUtcTimestamp("2026-11-11T25:07:05Z"));
    CHECK_FALSE(parseUtcTimestamp("2026-11-11T21:07:05.750"));
    CHECK(recordingFileName(when, 137'900'000) == "2026-11-11T210705Z_137900000Hz.cu8");
}

TEST_CASE("sidecar: JSON round-trips, including automatic gain and the sample count") {
    IqMetadata metadata = sampleMetadata();
    std::string error;
    auto parsed = metadataFromJson(toJson(metadata), error);
    REQUIRE(parsed);
    CHECK(parsed->centerFrequencyHz == metadata.centerFrequencyHz);
    CHECK(parsed->sampleRateSps == metadata.sampleRateSps);
    CHECK(parsed->startUtc == metadata.startUtc);
    CHECK(parsed->gainTenthsDb == 297);
    CHECK(parsed->ppm == -2);
    CHECK(parsed->device == "synthetic");
    CHECK_FALSE(parsed->sampleCount);

    metadata.gainTenthsDb.reset();
    metadata.sampleCount = 123'456;
    parsed = metadataFromJson(toJson(metadata), error);
    REQUIRE(parsed);
    CHECK_FALSE(parsed->gainTenthsDb);
    CHECK(parsed->sampleCount == 123'456U);
}

TEST_CASE("sidecar: missing or wrong fields are refused with a reason") {
    std::string error;
    CHECK_FALSE(metadataFromJson("{", error));
    CHECK(error.find("JSON") != std::string::npos);
    CHECK_FALSE(metadataFromJson(R"({"format":"cs16","centerFrequencyHz":1,"sampleRateSps":1,
                                      "startUtc":"2026-11-11T21:07:05Z"})",
                                 error));
    CHECK(error.find("cu8") != std::string::npos);
    CHECK_FALSE(metadataFromJson(R"({"format":"cu8","sampleRateSps":1,
                                      "startUtc":"2026-11-11T21:07:05Z"})",
                                 error));
    CHECK_FALSE(metadataFromJson(R"({"format":"cu8","centerFrequencyHz":1,"sampleRateSps":0,
                                      "startUtc":"2026-11-11T21:07:05Z"})",
                                 error));
    CHECK_FALSE(metadataFromJson(R"({"format":"cu8","centerFrequencyHz":1,"sampleRateSps":1,
                                      "startUtc":"yesterday"})",
                                 error));
    // Implausible values (corrupt or crafted files) are refused.
    CHECK_FALSE(metadataFromJson(R"({"format":"cu8","centerFrequencyHz":1000000000000000000,
                                      "sampleRateSps":2400000,"startUtc":"2026-11-11T21:07:05Z"})",
                                 error));
    CHECK_FALSE(metadataFromJson(R"({"format":"cu8","centerFrequencyHz":-5,"sampleRateSps":2400000,
                                      "startUtc":"2026-11-11T21:07:05Z"})",
                                 error));
    CHECK_FALSE(metadataFromJson(R"({"format":"cu8","centerFrequencyHz":100000000,"sampleRateSps":5,
                                      "startUtc":"2026-11-11T21:07:05Z"})",
                                 error));
    CHECK(sidecarPath("/x/rec.cu8") == std::filesystem::path("/x/rec.json"));
}

TEST_CASE("file source: a recording plays back byte for byte, with its metadata") {
    const TempDir dir;
    const IqMetadata metadata = sampleMetadata();
    // 2.5 blocks, so the last block is a partial one.
    const auto bytes = syntheticBytes(kBlockBytes * 5 / 2, metadata.sampleRateSps);
    const auto path = dir.path / "tone.cu8";
    writeRecording(path, bytes, metadata);

    const auto blocks = playOnce(path);
    REQUIRE(blocks.size() == 3);
    std::vector<uint8_t> played;
    uint64_t samplesBefore = 0;
    for (size_t i = 0; i < blocks.size(); ++i) {
        CAPTURE(i);
        CHECK(blocks[i].sequence == i);
        CHECK(blocks[i].centerFrequencyHz == metadata.centerFrequencyHz);
        CHECK(blocks[i].sampleRateSps == metadata.sampleRateSps);
        // Block timestamps continue from the recording's start.
        const auto expected =
            metadata.startUtc + duration_cast<system_clock::duration>(duration<double>(
                                    static_cast<double>(samplesBefore) / metadata.sampleRateSps));
        CHECK(abs(blocks[i].firstSampleUtc - expected) < microseconds{2});
        samplesBefore += blocks[i].bytes.size() / 2;
        played.insert(played.end(), blocks[i].bytes.begin(), blocks[i].bytes.end());
    }
    CHECK(played == bytes);
}

TEST_CASE("file source: a recording without a sidecar is refused") {
    const TempDir dir;
    const auto path = dir.path / "orphan.cu8";
    std::ofstream(path, std::ios::binary) << "abcd";
    CHECK_THROWS_WITH(FileSource(path, {}), doctest::Contains("sidecar"));
}

TEST_CASE("file source: real-time playback paces blocks and loops") {
    const TempDir dir;
    IqMetadata metadata = sampleMetadata();
    metadata.sampleRateSps = 2'048'000;  // one 64 KiB block = 16 ms
    const auto path = dir.path / "short.cu8";
    writeRecording(path, syntheticBytes(kBlockBytes, metadata.sampleRateSps), metadata);

    FileSource source(path, {.realTime = true, .loop = true});
    BlockQueue queue = makeBlockQueue(64);
    std::string error;
    REQUIRE(source.start(queue, error));
    const auto begin = steady_clock::now();
    std::this_thread::sleep_for(milliseconds{200});
    source.stop();
    const auto elapsed = duration<double>(steady_clock::now() - begin).count();
    size_t received = 0;
    while (queue.beginRead() != nullptr) {
        queue.commitRead();
        ++received;
    }
    // 16 ms per block: about 12 in 200 ms. Generous bounds for a busy machine.
    const double expected = elapsed / 0.016;
    CHECK(static_cast<double>(received) > expected * 0.5);
    CHECK(static_cast<double>(received) < expected * 1.5 + 2);
    CHECK_FALSE(source.finished());  // looping never finishes by itself
}

TEST_CASE("recorder: writes the blocks and a sidecar, and the result replays identically (M1)") {
    const TempDir dir;
    const IqMetadata metadata = sampleMetadata();
    const auto source = dir.path / "source.cu8";
    const auto bytes = syntheticBytes(kBlockBytes * 4, metadata.sampleRateSps);
    writeRecording(source, bytes, metadata);

    // Play the file into the recorder, the way the DSP thread will.
    IqRecorder recorder;
    IqMetadata recordingMeta = metadata;
    recordingMeta.startUtc = system_clock::now();
    REQUIRE(recorder.start(dir.path / "recordings", recordingMeta));
    CHECK(recorder.status().state == RecorderState::Recording);
    for (const SampleBlock& block : playOnce(source)) {
        recorder.offer(block);
    }
    // Let the writer catch up, then stop.
    for (int i = 0; i < 200 && recorder.status().bytesWritten < bytes.size(); ++i) {
        std::this_thread::sleep_for(milliseconds{5});
    }
    recorder.requestStop();
    recorder.waitUntilIdle();

    const RecorderStatus status = recorder.status();
    CHECK(status.state == RecorderState::Idle);
    CHECK(status.error.empty());
    CHECK(status.bytesWritten == bytes.size());
    CHECK(status.droppedBlocks == 0);
    CHECK_FALSE(status.endedByRetune);
    REQUIRE(std::filesystem::exists(status.path));
    CHECK(readAll(status.path) == bytes);

    std::string error;
    auto sidecar = readSidecar(status.path, error);
    REQUIRE(sidecar);
    CHECK(sidecar->sampleCount == bytes.size() / 2);
    CHECK(sidecar->centerFrequencyHz == metadata.centerFrequencyHz);
    // The start time is the first sample's, taken from the blocks.
    CHECK(sidecar->startUtc == floor<milliseconds>(metadata.startUtc));

    // The recording plays back exactly what went in.
    std::vector<uint8_t> replayed;
    for (const SampleBlock& block : playOnce(status.path)) {
        replayed.insert(replayed.end(), block.bytes.begin(), block.bytes.end());
    }
    CHECK(replayed == bytes);
}

TEST_CASE("recorder: a retune ends the recording") {
    const TempDir dir;
    IqRecorder recorder;
    IqMetadata metadata = sampleMetadata();
    REQUIRE(recorder.start(dir.path, metadata));

    SampleBlock block;
    block.bytes.assign(1024, 127);
    block.centerFrequencyHz = metadata.centerFrequencyHz;
    block.sampleRateSps = metadata.sampleRateSps;
    block.firstSampleUtc = metadata.startUtc;
    recorder.offer(block);
    block.centerFrequencyHz += 1'000'000;
    recorder.offer(block);
    recorder.waitUntilIdle();

    const auto status = recorder.status();
    CHECK(status.endedByRetune);
    CHECK(status.bytesWritten == 1024);
    CHECK(status.state == RecorderState::Idle);
    // Starting again works once the previous recording has finished.
    CHECK(recorder.start(dir.path, metadata));
    recorder.requestStop();
    recorder.waitUntilIdle();
}
