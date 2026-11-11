// SPDX-License-Identifier: GPL-3.0-or-later
//
// [hardware] tests: they need an RTL-SDR stick plugged in, so they live in
// their own executable that `ctest` never runs. Run them by hand:
//
//   build/tests/zarescope_hardware_tests
//
// They report what the stick says it is (the authenticity check for a new
// stick), then stream a second of samples and check the rate.

#include "device/Receiver.h"
#include "device/RtlSdrSource.h"

#include <doctest/doctest.h>

#include <thread>

using namespace zarescope::device;
using namespace std::chrono;

TEST_CASE("[hardware] a stick is connected and identifies itself") {
    const auto devices = listRtlSdrDevices();
    INFO("No RTL-SDR stick found. Plug one in and run again.");
    REQUIRE_FALSE(devices.empty());
    for (const auto& device : devices) {
        MESSAGE("#" << device.index << " " << device.name << " — manufacturer \""
                    << device.manufacturer << "\", product \"" << device.product << "\", serial \""
                    << device.serial << "\"");
    }

    RtlSdrSource source(0, RtlSdrSettings{});
    BlockQueue queue = makeBlockQueue();
    std::string error;
    REQUIRE_MESSAGE(source.start(queue, error), error);
    const DeviceIdentity identity = source.identity();
    MESSAGE("Tuner: " << tunerName(identity.tuner));
    MESSAGE("EEPROM claims: " << (identity.claimed == DongleModel::Other
                                      ? "no RTL-SDR Blog model"
                                      : modelName(identity.claimed)));
    switch (identity.verdict) {
    case IdentityVerdict::Consistent:
        MESSAGE(
            "Verdict: the tuner matches the claimed model. (Necessary for a genuine stick, "
            "not proof: also check HF reception below 24 MHz on a V4.)");
        break;
    case IdentityVerdict::Mismatch:
        MESSAGE(
            "Verdict: MISMATCH. The EEPROM claims a model this tuner doesn't belong to. "
            "Likely counterfeit.");
        break;
    case IdentityVerdict::Unverifiable:
        MESSAGE("Verdict: generic stick, nothing to check against.");
        break;
    }
    CHECK(identity.verdict != IdentityVerdict::Mismatch);
    CHECK_FALSE(source.biasTee());  // always off after opening
    MESSAGE("Gains (tenths of dB): " << source.supportedGainsTenthsDb().size() << " steps");
    source.stop();
}

TEST_CASE("[hardware] the stick streams at the requested sample rate") {
    RtlSdrSource source(
        0, RtlSdrSettings{.centerFrequencyHz = 100'000'000, .sampleRateSps = 2'400'000});
    BlockQueue queue = makeBlockQueue();
    std::string error;
    REQUIRE_MESSAGE(source.start(queue, error), error);

    uint64_t bytes = 0;
    const auto begin = steady_clock::now();
    while (steady_clock::now() - begin < seconds{2}) {
        while (SampleBlock* block = queue.beginRead()) {
            bytes += block->bytes.size();
            queue.commitRead();
        }
        std::this_thread::sleep_for(milliseconds{5});
    }
    const double elapsed = duration<double>(steady_clock::now() - begin).count();
    source.stop();
    const double rate = static_cast<double>(bytes / 2) / elapsed;
    MESSAGE("Measured " << rate / 1e6 << " MS/s, dropped " << source.droppedBlocks() << " blocks");
    CHECK(rate == doctest::Approx(2'400'000).epsilon(0.05));
    CHECK(source.droppedBlocks() == 0);
    CHECK(source.lastError().empty());
}

TEST_CASE("[hardware] the receiver produces a spectrum from the stick") {
    Receiver receiver;
    receiver.openDevice(0, RtlSdrSettings{});
    receiver.waitForCommands();
    REQUIRE_MESSAGE(receiver.status().state == ReceiverState::Running, receiver.status().error);
    SpectrumRow trace;
    bool got = false;
    for (int i = 0; i < 200 && !got; ++i) {
        got = receiver.latestTrace(trace);
        std::this_thread::sleep_for(milliseconds{10});
    }
    CHECK(got);
    CHECK(trace.dbfs.size() == kDefaultFftSize);
    receiver.close();
    receiver.waitForCommands();
}
