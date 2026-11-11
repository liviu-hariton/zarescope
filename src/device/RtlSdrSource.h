// SPDX-License-Identifier: GPL-3.0-or-later
//
// RtlSdrSource: an RTL-SDR stick as a SampleSource, through librtlsdr (the
// RTL-SDR Blog fork).
//
// Threads:
//   reader thread   runs rtlsdr_read_async(); its callback copies each USB
//                   transfer into the block queue (no allocation, no locks)
//   caller          start(), stop() and the set...() controls. These make
//                   USB control transfers that can take tens of milliseconds,
//                   so they must not run on the UI thread; the Receiver's
//                   control thread calls them.
//
// Safety: the bias-tee (antenna power through the coax) is switched off
// explicitly on every open, whatever state the stick was left in, and only
// turns on through setBiasTee(true).

#pragma once

#include "device/DeviceIdentity.h"
#include "device/SampleSource.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct rtlsdr_dev;

namespace zarescope::device {

// A stick found on USB.
struct RtlSdrDeviceEntry {
    uint32_t index = 0;
    std::string name;  // librtlsdr's name for the USB ID, e.g. "Generic RTL2832U OEM"
    std::string manufacturer;
    std::string product;
    std::string serial;
};

// Lists the sticks on USB. Takes a few milliseconds (USB enumeration): not
// for the UI thread.
std::vector<RtlSdrDeviceEntry> listRtlSdrDevices();

struct RtlSdrSettings {
    int64_t centerFrequencyHz = 100'000'000;
    uint32_t sampleRateSps = 2'400'000;
    std::optional<int> gainTenthsDb;  // nullopt = the tuner's automatic gain
    int ppm = 0;
};

// Sample rates the RTL2832U handles reliably (2 MS/s is the one ADS-B
// decoding needs) (librtlsdr accepts
// 225 001–300 000 and 900 001–3 200 000; above 2.56 MS/s samples may drop).
inline constexpr uint32_t kSampleRates[] = {250'000,   1'024'000, 1'536'000, 1'792'000,
                                            1'920'000, 2'000'000, 2'048'000, 2'160'000,
                                            2'400'000, 2'560'000, 2'880'000, 3'200'000};

// The frequency range librtlsdr can tune (the R82xx tuners' upper limit).
inline constexpr int64_t kMinFrequencyHz = 500'000;
inline constexpr int64_t kMaxFrequencyHz = 1'766'000'000;

class RtlSdrSource final : public SampleSource {
public:
    RtlSdrSource(uint32_t deviceIndex, RtlSdrSettings settings);
    ~RtlSdrSource() override;
    RtlSdrSource(const RtlSdrSource&) = delete;
    RtlSdrSource& operator=(const RtlSdrSource&) = delete;
    RtlSdrSource(RtlSdrSource&&) = delete;
    RtlSdrSource& operator=(RtlSdrSource&&) = delete;

    // Opens the stick, applies the settings (bias-tee off), starts streaming.
    bool start(BlockQueue& queue, std::string& error) override;
    void stop() override;
    [[nodiscard]] SourceInfo info() const override;

    // Controls. Each returns false and fills `error` if librtlsdr refuses.
    bool setCenterFrequency(int64_t frequencyHz, std::string& error);
    bool setSampleRate(uint32_t sampleRateSps, std::string& error);
    // nullopt = automatic; otherwise the nearest gain the tuner supports.
    bool setGain(std::optional<int> gainTenthsDb, std::string& error);
    bool setPpm(int ppm, std::string& error);
    bool setBiasTee(bool on, std::string& error);

    // Filled in by start().
    [[nodiscard]] const std::vector<int>& supportedGainsTenthsDb() const { return m_gains; }
    [[nodiscard]] const RtlSdrDeviceEntry& entry() const { return m_entry; }
    [[nodiscard]] DeviceIdentity identity() const { return m_identity; }
    [[nodiscard]] RtlSdrSettings settings() const;
    [[nodiscard]] bool biasTee() const { return m_biasTee.load(); }

private:
    static void onSamples(unsigned char* buffer, uint32_t length, void* context);
    void closeDevice();

    uint32_t m_index;
    rtlsdr_dev* m_device = nullptr;
    BlockQueue* m_queue = nullptr;
    std::thread m_reader;

    RtlSdrDeviceEntry m_entry;
    DeviceIdentity m_identity;
    std::vector<int> m_gains;

    // Read by the reader thread for every block; written by the controls.
    std::atomic<int64_t> m_centerFrequencyHz;
    std::atomic<uint32_t> m_sampleRateSps;
    std::atomic<bool> m_biasTee{false};
    mutable std::mutex m_settingsMutex;  // guards m_settings (gain, ppm)
    RtlSdrSettings m_settings;
};

}  // namespace zarescope::device
