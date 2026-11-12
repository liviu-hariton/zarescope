// SPDX-License-Identifier: GPL-3.0-or-later
//
// M5: automatic pass recording and decoding.
//
//   PassScheduler    which pass is due, with a fake clock
//   DeviceArbiter    who owns the stick
//   runProcess       an external program, its output, exit code, timeout
//   SatDumpRunner    against a fake `satdump` (a shell script), and against
//                    the real one when it is installed
//   PassAutomation   a simulated pass end to end: a FileSource playing a
//                    137.9 MHz recording, a fake clock, the fake SatDump
//
// No stick needed; the real SatDump check skips itself when SatDump is missing.

#include "app/PassAutomation.h"
#include "device/DeviceArbiter.h"
#include "device/IqMetadata.h"
#include "device/IqRecorder.h"
#include "device/Receiver.h"
#include "integrations/SatDumpRunner.h"
#include "platform/Process.h"
#include "sat/PassScheduler.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <fstream>
#include <random>
#include <thread>

using namespace zarescope;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        std::random_device random;
        path = fs::temp_directory_path() /
               ("zarescope-test-" + std::to_string(random()) + std::to_string(random()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;
};

template <typename Predicate>
bool waitFor(Predicate done, std::chrono::milliseconds timeout = 5000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

sat::Satellite meteor(uint32_t norad, const std::string& name) {
    sat::Satellite s;
    s.noradId = norad;
    s.name = name;
    s.downlinks.push_back({137'900'000, "lrpt", 150'000});
    s.satdumpPipeline = "meteor_m2-x_lrpt";
    s.satdumpArgs = {"--dc_block"};
    s.recordSampleRateSps = 1'024'000;
    return s;
}

const sat::Clock::time_point kT0 = sat::Clock::from_time_t(1'800'000'000);  // 2027-01-15 08:00 UTC

sat::PlannedPass pass(uint32_t norad, const std::string& name, sat::Clock::duration aosFromT0,
                      std::chrono::minutes length, double maxElevationDeg) {
    sat::PlannedPass p;
    p.noradId = norad;
    p.satellite = name;
    p.pass.aos = kT0 + aosFromT0;
    p.pass.los = p.pass.aos + length;
    p.pass.tca = p.pass.aos + length / 2;
    p.pass.maxElevationDeg = maxElevationDeg;
    return p;
}

// A fake `satdump`: prints what the real one prints, makes an image in the
// output folder ($4), and notes its arguments next to the recording ($3).
// `mode`: "ok" makes an image, "empty" makes none, "slow" sleeps.
fs::path writeFakeSatDump(const fs::path& dir, const std::string& mode) {
    const fs::path script = dir / ("satdump-" + mode);
    std::ofstream out(script);
    out << "#!/bin/sh\n"
           "echo \"$@\" > \"$(dirname \"$3\")/args.txt\"\n"
           "printf '[06:42 - 12/11/2026] \\033[32m(I) Starting SatDump v1.2.2\\033[m\\n'\n"
           "echo '(I) Progress 4.500000%, SNR : 6.2dB, Peak SNR: 8.0dB'\n"
           "echo '(I) Progress nan%, Viterbi : SYNC'\n";
    if (mode == "ok") {
        out << "mkdir -p \"$4/MSU-MR\"\n"
               "printf 'png' > \"$4/MSU-MR/rgb_msu_mr_221.png\"\n"
               "printf 'png' > \"$4/MSU-MR/msu_mr-1.png\"\n"
               "printf 'frames' > \"$4/meteor_m2-x_lrpt.cadu\"\n";
    } else if (mode == "slow") {
        out << "sleep 30\n";
    }
    out << "echo '(I) Done! Goodbye'\n";
    out.close();
    fs::permissions(script, fs::perms::owner_all);
    return script;
}

// A short recording on 137.9 MHz at 1.024 MS/s: noise is enough, SatDump
// (fake or real) only needs a file.
fs::path writeMeteorRecording(const fs::path& dir, size_t bytes) {
    const fs::path path = dir / "meteor.cu8";
    std::vector<char> data(bytes);
    std::mt19937 random(1);
    for (char& c : data) {
        c = static_cast<char>(random() & 0xFF);
    }
    std::ofstream(path, std::ios::binary).write(data.data(), static_cast<std::streamsize>(bytes));
    device::IqMetadata metadata;
    metadata.centerFrequencyHz = 137'900'000;
    metadata.sampleRateSps = 1'024'000;
    metadata.startUtc = kT0;
    metadata.software = "test";
    std::string error;
    REQUIRE(device::writeSidecar(path, metadata, error));
    return path;
}

}  // namespace

// --- IqRecorder (the parts M5 relies on) ------------------------------------------

TEST_CASE("recorder: blocks from before a retune are skipped, a retune after the start ends it") {
    const TempDir dir;
    device::IqRecorder recorder;
    device::IqMetadata metadata;
    metadata.centerFrequencyHz = 137'900'000;
    metadata.sampleRateSps = 1'024'000;
    metadata.startUtc = kT0;
    REQUIRE(recorder.start(dir.path, metadata));

    device::SampleBlock block;
    block.bytes.assign(1024, 127);
    block.sampleRateSps = 1'024'000;
    block.centerFrequencyHz = 100'000'000;  // still queued from before the retune
    recorder.offer(block);
    recorder.offer(block);
    block.centerFrequencyHz = 137'900'000;
    recorder.offer(block);
    CHECK(recorder.status().state == device::RecorderState::Recording);
    block.centerFrequencyHz = 100'000'000;  // the user retunes during the recording
    recorder.offer(block);
    recorder.waitUntilIdle();
    const auto status = recorder.status();
    CHECK(status.endedByRetune);
    CHECK(status.bytesWritten == 1024);  // only the matching block
}

TEST_CASE("recorder: a recording that can't fit on the disk fails at once") {
    const TempDir dir;
    device::IqRecorder recorder;
    device::IqMetadata metadata;
    metadata.centerFrequencyHz = 137'900'000;
    metadata.sampleRateSps = 1'024'000;
    metadata.startUtc = kT0;
    REQUIRE(recorder.start(dir.path, metadata, uint64_t{1} << 60));  // an exabyte
    recorder.waitUntilIdle();
    CHECK(recorder.status().state == device::RecorderState::Idle);
    CHECK(recorder.status().error.find("not enough free disk space") != std::string::npos);
}

// --- PassScheduler -------------------------------------------------------------

TEST_CASE("scheduler: only satellites that can be decoded are eligible") {
    auto inactive = meteor(1, "Old");
    inactive.active = false;
    auto noPipeline = meteor(2, "No pipeline");
    noPipeline.satdumpPipeline.clear();
    auto noLrpt = meteor(3, "APT only");
    noLrpt.downlinks = {{137'100'000, "apt", 40'000}};
    const sat::PassScheduler scheduler({inactive, noPipeline, noLrpt, meteor(59051, "Meteor-M N2-4")});

    CHECK_FALSE(scheduler.planFor(pass(1, "Old", 0min, 15min, 50)));
    CHECK_FALSE(scheduler.planFor(pass(2, "No pipeline", 0min, 15min, 50)));
    CHECK_FALSE(scheduler.planFor(pass(3, "APT only", 0min, 15min, 50)));
    CHECK_FALSE(scheduler.planFor(pass(999, "Unknown", 0min, 15min, 50)));
    const auto plan = scheduler.planFor(pass(59051, "Meteor-M N2-4", 0min, 15min, 50));
    REQUIRE(plan);
    CHECK(plan->frequencyHz == 137'900'000);  // from the catalog
    CHECK(plan->sampleRateSps == 1'024'000);
    CHECK(plan->pipeline == "meteor_m2-x_lrpt");
    CHECK(plan->satdumpArgs == std::vector<std::string>{"--dc_block"});
}

TEST_CASE("scheduler: a pass is due from AOS while enough of it is left") {
    sat::PassScheduler scheduler({meteor(59051, "N2-4")});
    const std::vector passes{pass(59051, "N2-4", 10min, 14min, 60)};

    CHECK_FALSE(scheduler.due(kT0, passes));               // before AOS
    CHECK(scheduler.due(kT0 + 10min, passes));             // at AOS
    CHECK(scheduler.due(kT0 + 21min, passes));             // late, 3 minutes left
    CHECK_FALSE(scheduler.due(kT0 + 23min, passes));       // 1 minute left: not worth it
    CHECK_FALSE(scheduler.due(kT0 + 25min, passes));       // over
    REQUIRE(scheduler.next(kT0, passes));
    CHECK(scheduler.next(kT0, passes)->pass.aos == kT0 + 10min);
    CHECK_FALSE(scheduler.next(kT0 + 11min, passes));      // started already: not "next"

    // A short pass (simulated) is due from its AOS through its first half.
    const std::vector shortPass{pass(59051, "N2-4", 0min, 1min, 60)};
    CHECK(scheduler.due(kT0, shortPass));
    CHECK(scheduler.due(kT0 + 29s, shortPass));
    CHECK_FALSE(scheduler.due(kT0 + 31s, shortPass));
}

TEST_CASE("scheduler: overlapping passes go to the higher one, done passes never return") {
    sat::PassScheduler scheduler({meteor(57166, "N2-3"), meteor(59051, "N2-4")});
    std::vector passes{pass(57166, "N2-3", 0min, 14min, 25), pass(59051, "N2-4", 3min, 14min, 70)};

    const auto first = scheduler.due(kT0 + 5min, passes);
    REQUIRE(first);
    CHECK(first->pass.noradId == 59051);

    scheduler.markDone(first->pass);
    // The same pass, re-predicted a few seconds later after a TLE refresh.
    passes[1].pass.aos += 4s;
    const auto second = scheduler.due(kT0 + 5min, passes);
    REQUIRE(second);
    CHECK(second->pass.noradId == 57166);
    scheduler.markDone(second->pass);
    CHECK_FALSE(scheduler.due(kT0 + 5min, passes));
}

TEST_CASE("scheduler: product folders are named by AOS (UTC) and satellite") {
    CHECK(sat::productFolderName(kT0 + 3h + 30min + 59s, "Meteor-M N2-4") ==
          "2027-01-15_1130Z_Meteor-M_N2-4");
    CHECK(sat::productFolderName(kT0, "a/b  c:d") == "2027-01-15_0800Z_ab_cd");
}

// --- DeviceArbiter ---------------------------------------------------------------

TEST_CASE("arbiter: a pass takes the stick from the user and gives it back") {
    device::DeviceArbiter arbiter;
    CHECK(arbiter.state() == device::DeviceState::Idle);
    CHECK(arbiter.userMayControl());

    arbiter.sourceOpened();
    CHECK(arbiter.owner() == device::DeviceOwner::User);
    REQUIRE(arbiter.claimForPass());
    CHECK(arbiter.owner() == device::DeviceOwner::Scheduler);
    CHECK_FALSE(arbiter.userMayControl());
    CHECK_FALSE(arbiter.claimForPass());  // one pass at a time
    arbiter.sourceClosed();               // the pass reopening the stick: ignored
    CHECK(arbiter.owner() == device::DeviceOwner::Scheduler);
    CHECK_FALSE(arbiter.beginHandoff());  // not while a pass holds it
    arbiter.releasePass();
    CHECK(arbiter.state() == device::DeviceState::Live);
    CHECK(arbiter.owner() == device::DeviceOwner::User);

    arbiter.sourceClosed();
    CHECK(arbiter.state() == device::DeviceState::Idle);
    REQUIRE(arbiter.claimForPass());  // from Idle: the pass opens the stick itself
    arbiter.releasePass();
    CHECK(arbiter.state() == device::DeviceState::Idle);  // and closes it again
}

TEST_CASE("arbiter: handoff to an external process (v2) goes through every state") {
    device::DeviceArbiter arbiter;
    arbiter.sourceOpened();
    CHECK_FALSE(arbiter.handoffDone());  // wrong order: nothing changes
    REQUIRE(arbiter.beginHandoff());
    CHECK(arbiter.state() == device::DeviceState::HandoffPending);
    CHECK_FALSE(arbiter.userMayControl());
    REQUIRE(arbiter.handoffDone());
    CHECK(arbiter.state() == device::DeviceState::External);
    CHECK_FALSE(arbiter.claimForPass());  // the stick is in another process
    REQUIRE(arbiter.beginReclaim());
    REQUIRE(arbiter.reclaimDone());
    CHECK(arbiter.state() == device::DeviceState::Live);
    CHECK(arbiter.owner() == device::DeviceOwner::User);
}

// --- runProcess --------------------------------------------------------------------

TEST_CASE("process: output lines, exit code, a missing program") {
    const TempDir dir;
    std::vector<std::string> lines;
    const std::atomic<bool> cancel{false};
    const auto result = platform::runProcess(
        "/bin/sh", {"-c", "echo one; echo two >&2; printf three; exit 3"}, dir.path, 10s,
        [&](std::string_view line) { lines.emplace_back(line); }, cancel);
    CHECK(result.started);
    CHECK(result.exitCode == 3);
    CHECK(lines == std::vector<std::string>{"one", "two", "three"});

    const auto missing = platform::runProcess(dir.path / "no-such-program", {}, dir.path, 10s,
                                              [](std::string_view) {}, cancel);
    CHECK_FALSE(missing.started);
    CHECK_FALSE(missing.error.empty());
}

TEST_CASE("process: a program that runs too long is stopped") {
    const TempDir dir;
    const std::atomic<bool> cancel{false};
    const auto start = std::chrono::steady_clock::now();
    const auto result = platform::runProcess("/bin/sh", {"-c", "sleep 30"}, dir.path, 1s,
                                             [](std::string_view) {}, cancel);
    CHECK(result.timedOut);
    CHECK(std::chrono::steady_clock::now() - start < 10s);
}

// --- SatDumpRunner -------------------------------------------------------------------

TEST_CASE("satdump: log lines are understood") {
    using integrations::parseProgress;
    CHECK(parseProgress("[06:43 - 12/11/2026] (I) Progress 4.500000%, SNR : 0.0dB").value() ==
          doctest::Approx(4.5));
    CHECK(parseProgress("(I) Progress 100%").value() == doctest::Approx(100.0));
    CHECK_FALSE(parseProgress("(I) Progress nan%, Viterbi : NOSYNC"));
    CHECK_FALSE(parseProgress("(I) Progress inf%"));
    CHECK_FALSE(parseProgress("(I) Demodulation finished"));
    CHECK(integrations::parseVersion("(I) Starting SatDump v1.2.2").value() == "1.2.2");
    CHECK(integrations::stripAnsi("\x1b[31m\x1b[1m(E) bad\x1b[m") == "(E) bad");

    integrations::SatDumpJob job;
    job.baseband = "/data/pass.cu8";
    job.pipeline = "meteor_m2-x_lrpt";
    job.sampleRateSps = 1'024'000;
    job.extraArgs = {"--dc_block"};
    // Checked against SatDump 1.2.2's usage line (docs/DECISIONS.md).
    CHECK(integrations::satDumpArguments(job, "/data/out") ==
          std::vector<std::string>{"meteor_m2-x_lrpt", "baseband", "/data/pass.cu8", "/data/out",
                                   "--samplerate", "1024000", "--baseband_format", "cu8",
                                   "--dc_block"});
}

TEST_CASE("satdump: images are collected, the log and pass.json saved, the baseband removed") {
    const TempDir dir;
    const fs::path baseband = writeMeteorRecording(dir.path, 4096);
    integrations::SatDumpRunner runner({dir.path / "absent", writeFakeSatDump(dir.path, "ok")});

    integrations::SatDumpJob job;
    job.baseband = baseband;
    job.productsDir = dir.path / "products" / "pass";
    job.pipeline = "meteor_m2-x_lrpt";
    job.sampleRateSps = 1'024'000;
    job.extraArgs = {"--dc_block"};
    job.passInfo["satellite"] = "Meteor-M N2-4";
    std::atomic<int> calls{0};
    integrations::SatDumpResult result;
    runner.submit(job, [&](const integrations::SatDumpResult& r) {
        result = r;
        ++calls;
    });
    runner.waitIdle();

    CHECK(calls == 1);
    CHECK(result.ok);
    CHECK(result.error.empty());
    REQUIRE(result.images.size() == 2);  // the .cadu isn't an image
    CHECK(fs::exists(job.productsDir / "MSU-MR" / "rgb_msu_mr_221.png"));
    CHECK(runner.status().version == "1.2.2");

    std::ifstream log(result.log);
    const std::string logText((std::istreambuf_iterator<char>(log)), {});
    CHECK(logText.find("Starting SatDump v1.2.2") != std::string::npos);
    CHECK(logText.find('\x1b') == std::string::npos);  // colours stripped

    const auto info = nlohmann::json::parse(std::ifstream(job.productsDir / "pass.json"));
    CHECK(info["satellite"] == "Meteor-M N2-4");
    CHECK(info["decode"]["images"].size() == 2);
    CHECK(info["decode"]["satdumpVersion"] == "1.2.2");

    std::ifstream argsFile(dir.path / "args.txt");
    std::string args;
    std::getline(argsFile, args);
    CHECK(args.find("meteor_m2-x_lrpt baseband " + baseband.string()) == 0);
    CHECK(args.find("--samplerate 1024000 --baseband_format cu8 --dc_block") != std::string::npos);

    CHECK_FALSE(fs::exists(baseband));  // decoded: the 2 MB/s recording goes
    CHECK_FALSE(fs::exists(device::sidecarPath(baseband)));
    CHECK_FALSE(fs::exists(dir.path / "meteor.satdump"));  // scratch folder gone
}

TEST_CASE("satdump: no image keeps the recording, a missing SatDump fails once") {
    const TempDir dir;
    const fs::path baseband = writeMeteorRecording(dir.path, 4096);
    {
        integrations::SatDumpRunner runner({writeFakeSatDump(dir.path, "empty")});
        integrations::SatDumpJob job;
        job.baseband = baseband;
        job.productsDir = dir.path / "products";
        job.pipeline = "meteor_m2-x_lrpt";
        job.sampleRateSps = 1'024'000;
        integrations::SatDumpResult result;
        runner.submit(job, [&](const integrations::SatDumpResult& r) { result = r; });
        runner.waitIdle();
        CHECK_FALSE(result.ok);
        CHECK(result.exitCode == 0);  // SatDump says 0 even then
        CHECK(result.failure == integrations::SatDumpFailure::NoImage);
        CHECK(fs::exists(baseband));  // kept for another try
    }
    {
        integrations::SatDumpRunner runner({dir.path / "not-installed"});
        runner.waitIdle();
        CHECK(runner.status().probed);
        CHECK_FALSE(runner.status().executable);
        std::atomic<int> calls{0};
        std::string error;
        integrations::SatDumpFailure failure{};
        integrations::SatDumpJob job;
        job.baseband = baseband;
        job.productsDir = dir.path / "products2";
        runner.submit(job, [&](const integrations::SatDumpResult& r) {
            error = r.error;
            failure = r.failure;
            ++calls;
        });
        runner.waitIdle();
        CHECK(calls == 1);
        CHECK(error == "SatDump isn't installed");
        CHECK(failure == integrations::SatDumpFailure::NotInstalled);
    }
}

TEST_CASE("satdump: shutting down stops SatDump and still answers every job once") {
    const TempDir dir;
    const fs::path baseband = writeMeteorRecording(dir.path, 4096);
    std::atomic<int> calls{0};
    {
        integrations::SatDumpRunner runner({writeFakeSatDump(dir.path, "slow")});
        for (int i = 0; i < 3; ++i) {
            integrations::SatDumpJob job;
            job.baseband = baseband;
            job.productsDir = dir.path / ("products" + std::to_string(i));
            runner.submit(job, [&](const integrations::SatDumpResult&) { ++calls; });
        }
        REQUIRE(waitFor([&] { return runner.status().running; }));
    }  // the destructor cancels
    CHECK(calls == 3);
}

TEST_CASE("satdump: the installed SatDump accepts our command line (skipped if absent)") {
    integrations::SatDumpRunner probe(platform::satDumpCandidates());
    probe.waitIdle();
    if (!probe.status().executable) {
        MESSAGE("SatDump isn't installed: skipped");
        return;
    }
    const TempDir dir;
    // One second of noise: SatDump runs every stage and finds nothing.
    const fs::path baseband = writeMeteorRecording(dir.path, 2 * 1'024'000);
    integrations::SatDumpJob job;
    job.baseband = baseband;
    job.productsDir = dir.path / "products";
    job.pipeline = "meteor_m2-x_lrpt";
    job.sampleRateSps = 1'024'000;
    job.extraArgs = {"--dc_block"};
    job.timeout = 120s;
    integrations::SatDumpResult result;
    probe.submit(job, [&](const integrations::SatDumpResult& r) { result = r; });
    probe.waitIdle();
    CHECK(result.exitCode == 0);
    CHECK_FALSE(result.timedOut);
    CHECK_FALSE(result.ok);  // noise has no image in it
    CHECK_FALSE(probe.status().version.empty());
    std::ifstream log(result.log);
    const std::string text((std::istreambuf_iterator<char>(log)), {});
    CHECK(text.find("Done! Goodbye") != std::string::npos);
    CHECK(text.find("Couldn't load config") == std::string::npos);  // found its resources
}

// --- PassAutomation ----------------------------------------------------------------

TEST_CASE("automation: a simulated pass is recorded from a file and decoded") {
    const TempDir dir;
    // 1 MB of 137.9 MHz "recording", played as fast as possible, looping.
    const fs::path source = writeMeteorRecording(dir.path, 1'000'000);
    device::Receiver receiver;
    receiver.openFile(source, {.realTime = false, .loop = true});
    REQUIRE(waitFor([&] { return receiver.status().state == device::ReceiverState::Running; }));

    app::AutomationConfig config;
    config.recordings = dir.path / "recordings";
    config.products = dir.path / "products";
    config.satDumpCandidates = {writeFakeSatDump(dir.path, "ok")};
    config.software = "Zarescope test";
    config.minRecording = 1s;
    app::PassAutomation automation(receiver, {meteor(59051, "Meteor-M N2-4")}, config);

    const std::vector passes{pass(59051, "Meteor-M N2-4", 1min, 12min, 72)};
    automation.update(kT0, passes);
    CHECK(automation.status().phase == app::AutomationPhase::Off);  // not enabled yet
    automation.setEnabled(true);
    automation.update(kT0, passes);
    CHECK(automation.status().phase == app::AutomationPhase::Waiting);
    REQUIRE(automation.status().next);

    // AOS (fake clock): the stick, here a file, is claimed and recorded.
    sat::Clock::time_point now = kT0 + 1min;
    automation.update(now, passes);
    CHECK(automation.status().phase == app::AutomationPhase::Preparing);
    CHECK_FALSE(automation.userMayControl());
    REQUIRE(waitFor([&] {
        automation.update(now, passes);
        return automation.status().phase == app::AutomationPhase::Recording;
    }));
    // Enough data for the minimum (1 s = 2 MB at 1.024 MS/s).
    REQUIRE(waitFor([&] {
        automation.update(now, passes);
        return automation.status().bytesRecorded > 3'000'000;
    }));

    // LOS: stop, give the receiver back, decode.
    now = kT0 + 13min;
    REQUIRE(waitFor([&] {
        automation.update(now, passes);
        return automation.status().phase == app::AutomationPhase::Decoding;
    }));
    CHECK(automation.userMayControl());
    automation.waitForDecoding();
    automation.update(now, passes);

    const auto status = automation.status();
    CHECK(status.phase == app::AutomationPhase::Waiting);
    REQUIRE(status.last);
    CHECK(status.last->ok);
    CHECK(status.last->failure == app::PassFailure::None);
    CHECK(status.last->images == 2);
    CHECK(status.last->satellite == "Meteor-M N2-4");
    const fs::path folder = config.products / sat::productFolderName(kT0 + 1min, "Meteor-M N2-4");
    CHECK(status.last->folder == folder);
    CHECK(fs::exists(folder / "MSU-MR" / "msu_mr-1.png"));
    const auto info = nlohmann::json::parse(std::ifstream(folder / "pass.json"));
    CHECK(info["noradId"] == 59051);
    CHECK(info["frequencyHz"] == 137'900'000);
    CHECK(info["recordingBytes"].get<uint64_t>() > 3'000'000);
    // The recording was decoded, so it was removed; the file being played stays.
    size_t leftover = 0;  // the fake SatDump's args.txt aside
    for (const auto& entry : fs::directory_iterator(config.recordings)) {
        leftover += entry.path().extension() == ".cu8" || entry.path().extension() == ".json";
    }
    CHECK(leftover == 0);
    CHECK(fs::exists(source));
    CHECK(receiver.status().state == device::ReceiverState::Running);

    // The same pass is never recorded twice.
    automation.update(kT0 + 2min, passes);
    CHECK(automation.status().phase == app::AutomationPhase::Waiting);
    CHECK(automation.status().last->serial == status.last->serial);
}

TEST_CASE("automation: a recording off the satellite's frequency gives the pass up") {
    const TempDir dir;
    const fs::path source = writeMeteorRecording(dir.path, 100'000);
    {
        device::IqMetadata metadata;
        metadata.centerFrequencyHz = 100'000'000;  // the FM band, not 137.9 MHz
        metadata.sampleRateSps = 1'024'000;
        metadata.startUtc = kT0;
        std::string error;
        REQUIRE(device::writeSidecar(source, metadata, error));
    }
    device::Receiver receiver;
    receiver.openFile(source, {.realTime = true, .loop = true});
    REQUIRE(waitFor([&] { return receiver.status().state == device::ReceiverState::Running; }));

    app::AutomationConfig config;
    config.recordings = dir.path / "recordings";
    config.products = dir.path / "products";
    config.satDumpCandidates = {writeFakeSatDump(dir.path, "ok")};
    app::PassAutomation automation(receiver, {meteor(59051, "N2-4")}, config);
    automation.setEnabled(true);
    const std::vector passes{pass(59051, "N2-4", 0min, 12min, 40)};
    automation.update(kT0, passes);
    automation.update(kT0, passes);
    const auto status = automation.status();
    CHECK(status.phase == app::AutomationPhase::Waiting);
    REQUIRE(status.last);
    CHECK_FALSE(status.last->ok);
    CHECK(status.last->failure == app::PassFailure::OffFrequency);
    CHECK(automation.userMayControl());
}
