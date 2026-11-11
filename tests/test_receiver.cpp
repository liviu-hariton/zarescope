// SPDX-License-Identifier: GPL-3.0-or-later
//
// The receive chain end to end, on recordings: source thread → DSP thread
// → rows and trace for the UI, plus the recorder tap. Also the stick
// identity check, which needs no stick.

#include "decoders/adsb/Encoder.h"
#include "device/DeviceIdentity.h"
#include "device/IqMetadata.h"
#include "device/Receiver.h"
#include "dsp/SignalGenerator.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <random>
#include <thread>

using namespace zarescope::device;
using namespace std::chrono;

namespace {

struct TempDir {
    std::filesystem::path path;
    TempDir() {
        std::random_device random;
        path = std::filesystem::temp_directory_path() /
               ("zarescope-rx-" + std::to_string(random()) + std::to_string(random()));
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

constexpr uint32_t kRate = 2'048'000;
constexpr int64_t kCenter = 98'000'000;
constexpr double kToneOffset = 256'000.0;  // exactly 512 bins at 4096 points

// Writes `seconds` of a tone at kToneOffset plus noise, with its sidecar.
std::filesystem::path writeToneRecording(const std::filesystem::path& dir, double seconds) {
    zarescope::dsp::SignalGenerator generator(kRate, 0.02F, 11);
    const std::array carriers{zarescope::dsp::Carrier{.offsetHz = kToneOffset, .amplitude = 0.5F}};
    std::vector<uint8_t> iq(static_cast<size_t>(seconds * kRate) * 2);
    generator.generate(carriers, iq);
    const auto path = dir / "tone.cu8";
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(iq.data()), static_cast<std::streamsize>(iq.size()));
    IqMetadata metadata;
    metadata.centerFrequencyHz = kCenter;
    metadata.sampleRateSps = kRate;
    metadata.startUtc = system_clock::now();
    std::string error;
    REQUIRE(writeSidecar(path, metadata, error));
    return path;
}

// Waits up to `timeout` for `done()`.
template <typename Predicate>
bool waitFor(Predicate done, milliseconds timeout = milliseconds{3000}) {
    const auto until = steady_clock::now() + timeout;
    while (!done()) {
        if (steady_clock::now() > until) {
            return false;
        }
        std::this_thread::sleep_for(milliseconds{5});
    }
    return true;
}

}  // namespace

TEST_CASE("receiver: a recording flows through to waterfall rows and the trace") {
    const TempDir dir;
    const auto path = writeToneRecording(dir.path, 0.5);
    Receiver receiver;
    receiver.openFile(path, {.realTime = false, .loop = false});
    receiver.waitForCommands();

    std::vector<SpectrumRow> rows;
    // The file is played once; collect rows until the receiver closes it.
    REQUIRE(waitFor([&] {
        receiver.drainRows([&](const SpectrumRow& row) { rows.push_back(row); });
        return receiver.status().state == ReceiverState::Closed && !rows.empty();
    }));
    receiver.drainRows([&](const SpectrumRow& row) { rows.push_back(row); });

    // 0.5 s at 30 rows/s: about 15 rows (rounding of frames per row aside).
    CHECK(rows.size() >= 12);
    CHECK(rows.size() <= 18);
    const SpectrumRow& last = rows.back();
    CHECK(last.centerFrequencyHz == kCenter);
    CHECK(last.sampleRateSps == kRate);
    REQUIRE(last.dbfs.size() == kDefaultFftSize);
    const auto peak =
        std::distance(last.dbfs.begin(), std::max_element(last.dbfs.begin(), last.dbfs.end()));
    CHECK(peak == static_cast<long>(kDefaultFftSize / 2 + 512));

    SpectrumRow trace;
    REQUIRE(receiver.latestTrace(trace));
    CHECK(trace.dbfs.size() == kDefaultFftSize);
    CHECK(trace.dbfs[kDefaultFftSize / 2 + 512] == doctest::Approx(-6.02).epsilon(0.05));
    CHECK_FALSE(receiver.latestTrace(trace));  // nothing newer
}

TEST_CASE("receiver: a missing recording fails with a reason") {
    const TempDir dir;
    Receiver receiver;
    receiver.openFile(dir.path / "nothing.cu8");
    receiver.waitForCommands();
    const auto status = receiver.status();
    CHECK(status.state == ReceiverState::Failed);
    CHECK(status.error.find("sidecar") != std::string::npos);
}

TEST_CASE("receiver: the FFT size can change while running") {
    const TempDir dir;
    const auto path = writeToneRecording(dir.path, 0.25);
    Receiver receiver;
    receiver.setFftSize(1024);
    receiver.setFftSize(1000);  // not offered: ignored
    receiver.openFile(path, {.realTime = true, .loop = true});
    receiver.waitForCommands();
    CHECK(receiver.status().fftSize == 1024);

    SpectrumRow trace;
    REQUIRE(waitFor([&] { return receiver.latestTrace(trace) && trace.dbfs.size() == 1024; }));
    receiver.setFftSize(8192);
    REQUIRE(waitFor([&] { return receiver.latestTrace(trace) && trace.dbfs.size() == 8192; }));
    // 256 kHz is 1024 bins at 8192 points.
    const auto peak =
        std::distance(trace.dbfs.begin(), std::max_element(trace.dbfs.begin(), trace.dbfs.end()));
    CHECK(peak == 4096 + 1024);
    // Device controls do nothing to a file.
    receiver.setCenterFrequency(100'000'000);
    receiver.waitForCommands();
    CHECK(receiver.status().source.centerFrequencyHz == kCenter);
    receiver.close();
    receiver.waitForCommands();
    CHECK(receiver.status().state == ReceiverState::Closed);
}

TEST_CASE("receiver: records what it receives") {
    const TempDir dir;
    const auto path = writeToneRecording(dir.path, 0.25);
    Receiver receiver;
    CHECK_FALSE(receiver.startRecording(dir.path / "out", "test"));  // nothing running
    receiver.openFile(path, {.realTime = true, .loop = true});
    receiver.waitForCommands();
    REQUIRE(receiver.startRecording(dir.path / "out", "Zarescope test"));
    std::this_thread::sleep_for(milliseconds{300});
    receiver.stopRecording();
    REQUIRE(waitFor([&] { return receiver.recording().state == RecorderState::Idle; }));

    const auto status = receiver.recording();
    CHECK(status.error.empty());
    CHECK(status.bytesWritten > kBlockBytes);
    std::string error;
    auto sidecar = readSidecar(status.path, error);
    REQUIRE(sidecar);
    CHECK(sidecar->centerFrequencyHz == kCenter);
    CHECK(sidecar->sampleRateSps == kRate);
    CHECK(sidecar->software == "Zarescope test");
    CHECK(sidecar->sampleCount == status.bytesWritten / 2);
    CHECK(std::filesystem::file_size(status.path) == status.bytesWritten);
}

TEST_CASE("identity: a stick's EEPROM claim is checked against its tuner") {
    // Genuine combinations.
    auto v4 = identifyDevice("RTLSDRBlog", "Blog V4", TunerType::R828D);
    CHECK(v4.claimed == DongleModel::BlogV4);
    CHECK(v4.verdict == IdentityVerdict::Consistent);
    CHECK(identifyDevice("RTLSDRBlog", "Blog V4L", TunerType::R820T).verdict ==
          IdentityVerdict::Consistent);
    CHECK(identifyDevice("RTLSDRBlog", "Blog V3", TunerType::R820T).verdict ==
          IdentityVerdict::Consistent);

    // A "V4" whose tuner isn't the R828D: the classic fake.
    auto fake = identifyDevice("RTLSDRBlog", "Blog V4", TunerType::R820T);
    CHECK(fake.claimed == DongleModel::BlogV4);
    CHECK(fake.verdict == IdentityVerdict::Mismatch);
    CHECK(identifyDevice("RTLSDRBlog", "Blog V3", TunerType::FC0012).verdict ==
          IdentityVerdict::Mismatch);

    // Generic sticks claim nothing, so there's nothing to check.
    auto generic = identifyDevice("Realtek", "RTL2838UHIDIR", TunerType::R820T);
    CHECK(generic.claimed == DongleModel::Other);
    CHECK(generic.verdict == IdentityVerdict::Unverifiable);
    // Matching is exact, as in librtlsdr.
    CHECK(identifyDevice("RTLSDRBlog", "Blog V4 ", TunerType::R828D).claimed == DongleModel::Other);

    CHECK(tunerName(TunerType::R828D) == "Rafael Micro R828D");
    CHECK(modelName(DongleModel::BlogV4) == "RTL-SDR Blog V4");
}

TEST_CASE("devices: listing the sticks works with none plugged in") {
    // Without hardware this returns an empty list; it must not crash.
    const auto devices = listRtlSdrDevices();
    for (const auto& device : devices) {
        CHECK_FALSE(device.name.empty());
    }
}

TEST_CASE("receiver: decodes ADS-B from a 2 MS/s recording at 1090 MHz (M3)") {
    const TempDir dir;
    // A few messages with gaps, then silence to fill the recording.
    std::vector<uint8_t> iq(4000, 128);
    const std::vector<zarescope::adsb::RawMessage> sent{
        zarescope::adsb::encodeIdentification(0x4B1234, "TAROM12"),
        zarescope::adsb::encodeAirbornePosition(0x4B1234, 44.5, 26.0, 33000, false),
        zarescope::adsb::encodeAirbornePosition(0x4B1234, 44.5, 26.0, 33000, true),
    };
    for (const auto& message : sent) {
        zarescope::adsb::modulate(message, 0.4F, 0.3F, iq);
        iq.insert(iq.end(), 3000, 128);
    }
    iq.resize(kBlockBytes * 3, 128);
    const auto path = dir.path / "adsb.cu8";
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(iq.data()), static_cast<std::streamsize>(iq.size()));
    IqMetadata metadata;
    metadata.centerFrequencyHz = 1'090'000'000;
    metadata.sampleRateSps = 2'000'000;
    metadata.startUtc = system_clock::now();
    std::string error;
    REQUIRE(writeSidecar(path, metadata, error));

    Receiver receiver;
    receiver.openFile(path, {.realTime = false, .loop = false});
    std::vector<std::string> received;
    REQUIRE(waitFor([&] {
        receiver.drainAdsb(
            [&](const AdsbMessage& m) { received.push_back(zarescope::adsb::toHex(m.message)); });
        return received.size() >= sent.size();
    }));
    for (size_t i = 0; i < sent.size(); ++i) {
        CHECK(received[i] == zarescope::adsb::toHex(sent[i]));
    }
    CHECK(receiver.status().adsbMessages >= sent.size());
}
