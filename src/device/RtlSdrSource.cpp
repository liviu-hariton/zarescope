// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/RtlSdrSource.h"

#include <rtl-sdr.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>

namespace zarescope::device {

namespace {

// librtlsdr's default number of USB transfers in flight.
constexpr uint32_t kTransfers = 15;

TunerType toTunerType(enum rtlsdr_tuner tuner) {
    switch (tuner) {
    case RTLSDR_TUNER_E4000: return TunerType::E4000;
    case RTLSDR_TUNER_FC0012: return TunerType::FC0012;
    case RTLSDR_TUNER_FC0013: return TunerType::FC0013;
    case RTLSDR_TUNER_FC2580: return TunerType::FC2580;
    case RTLSDR_TUNER_R820T: return TunerType::R820T;
    case RTLSDR_TUNER_R828D: return TunerType::R828D;
    case RTLSDR_TUNER_UNKNOWN: break;
    }
    return TunerType::Unknown;
}

std::string failure(const char* what, int code) {
    return std::string(what) + " failed (librtlsdr error " + std::to_string(code) + ")";
}

}  // namespace

std::vector<RtlSdrDeviceEntry> listRtlSdrDevices() {
    std::vector<RtlSdrDeviceEntry> devices;
    const uint32_t count = rtlsdr_get_device_count();
    for (uint32_t i = 0; i < count; ++i) {
        RtlSdrDeviceEntry entry;
        entry.index = i;
        if (const char* name = rtlsdr_get_device_name(i)) {
            entry.name = name;
        }
        std::array<char, 256> manufacturer{};
        std::array<char, 256> product{};
        std::array<char, 256> serial{};
        if (rtlsdr_get_device_usb_strings(i, manufacturer.data(), product.data(), serial.data()) ==
            0) {
            entry.manufacturer = manufacturer.data();
            entry.product = product.data();
            entry.serial = serial.data();
        }
        devices.push_back(std::move(entry));
    }
    return devices;
}

RtlSdrSource::RtlSdrSource(uint32_t deviceIndex, RtlSdrSettings settings)
    : m_index(deviceIndex),
      m_centerFrequencyHz(settings.centerFrequencyHz),
      m_sampleRateSps(settings.sampleRateSps),
      m_settings(settings) {}

RtlSdrSource::~RtlSdrSource() { stop(); }

bool RtlSdrSource::start(BlockQueue& queue, std::string& error) {
    stop();
    resetState();

    int result = rtlsdr_open(&m_device, m_index);
    if (result < 0 || m_device == nullptr) {
        m_device = nullptr;
        error = failure("Opening the receiver", result);
        return false;
    }

    // Identity: the EEPROM's USB strings and the tuner chip.
    m_entry.index = m_index;
    if (const char* name = rtlsdr_get_device_name(m_index)) {
        m_entry.name = name;
    }
    std::array<char, 256> manufacturer{};
    std::array<char, 256> product{};
    std::array<char, 256> serial{};
    if (rtlsdr_get_usb_strings(m_device, manufacturer.data(), product.data(), serial.data()) == 0) {
        m_entry.manufacturer = manufacturer.data();
        m_entry.product = product.data();
        m_entry.serial = serial.data();
    }
    m_identity = identifyDevice(m_entry.manufacturer, m_entry.product,
                                toTunerType(rtlsdr_get_tuner_type(m_device)));

    const int gainCount = rtlsdr_get_tuner_gains(m_device, nullptr);
    m_gains.assign(static_cast<size_t>(std::max(gainCount, 0)), 0);
    if (gainCount > 0) {
        rtlsdr_get_tuner_gains(m_device, m_gains.data());
    }

    // Safety first: antenna power off until the user turns it on.
    rtlsdr_set_bias_tee(m_device, 0);
    m_biasTee.store(false);

    const RtlSdrSettings settings = this->settings();
    if (!setSampleRate(settings.sampleRateSps, error) ||
        !setCenterFrequency(settings.centerFrequencyHz, error) ||
        !setGain(settings.gainTenthsDb, error) || !setPpm(settings.ppm, error)) {
        closeDevice();
        return false;
    }

    result = rtlsdr_reset_buffer(m_device);
    if (result < 0) {
        error = failure("Resetting the receiver's buffer", result);
        closeDevice();
        return false;
    }

    m_queue = &queue;
    m_reader = std::thread([this] {
        const int status = rtlsdr_read_async(m_device, &RtlSdrSource::onSamples, this, kTransfers,
                                             static_cast<uint32_t>(kBlockBytes));
        // read_async returns after cancel_async (status 0) or when the stick
        // goes away (negative).
        finish(status < 0 ? "The receiver stopped responding (unplugged?)" : std::string{});
    });
    return true;
}

void RtlSdrSource::stop() {
    if (m_device != nullptr && m_reader.joinable()) {
        rtlsdr_cancel_async(m_device);
    }
    if (m_reader.joinable()) {
        m_reader.join();
    }
    closeDevice();
}

void RtlSdrSource::closeDevice() {
    if (m_device != nullptr) {
        rtlsdr_set_bias_tee(m_device, 0);  // never leave antenna power on
        rtlsdr_close(m_device);
        m_device = nullptr;
    }
    m_biasTee.store(false);
}

void RtlSdrSource::onSamples(unsigned char* buffer, uint32_t length, void* context) {
    auto* self = static_cast<RtlSdrSource*>(context);
    const uint32_t rate = self->m_sampleRateSps.load(std::memory_order_relaxed);
    // The callback runs when the transfer completes, so its first sample was
    // taken length/2 samples ago.
    const auto now = std::chrono::system_clock::now();
    const auto span = std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::duration<double>(static_cast<double>(length / 2) / static_cast<double>(rate)));
    self->pushBlock(*self->m_queue, std::span<const uint8_t>(buffer, length),
                    self->m_centerFrequencyHz.load(std::memory_order_relaxed), rate, now - span);
}

SourceInfo RtlSdrSource::info() const {
    std::string name(modelName(m_identity.claimed));
    if (name.empty()) {
        name = m_entry.product.empty() ? m_entry.name : m_entry.product;
    }
    return {SourceKind::Device, name, m_centerFrequencyHz.load(), m_sampleRateSps.load()};
}

RtlSdrSettings RtlSdrSource::settings() const {
    const std::scoped_lock lock(m_settingsMutex);
    RtlSdrSettings settings = m_settings;
    settings.centerFrequencyHz = m_centerFrequencyHz.load();
    settings.sampleRateSps = m_sampleRateSps.load();
    return settings;
}

bool RtlSdrSource::setCenterFrequency(int64_t frequencyHz, std::string& error) {
    if (frequencyHz < kMinFrequencyHz || frequencyHz > kMaxFrequencyHz) {
        error = "frequency out of the receiver's range";
        return false;
    }
    if (m_device != nullptr) {
        const int result = rtlsdr_set_center_freq(m_device, static_cast<uint32_t>(frequencyHz));
        if (result < 0) {
            error = failure("Tuning", result);
            return false;
        }
    }
    m_centerFrequencyHz.store(frequencyHz);
    return true;
}

bool RtlSdrSource::setSampleRate(uint32_t sampleRateSps, std::string& error) {
    if (m_device != nullptr) {
        const int result = rtlsdr_set_sample_rate(m_device, sampleRateSps);
        if (result < 0) {
            error = failure("Setting the sample rate", result);
            return false;
        }
    }
    m_sampleRateSps.store(sampleRateSps);
    return true;
}

bool RtlSdrSource::setGain(std::optional<int> gainTenthsDb, std::string& error) {
    if (m_device != nullptr) {
        int result = rtlsdr_set_tuner_gain_mode(m_device, gainTenthsDb ? 1 : 0);
        if (result < 0) {
            error = failure("Setting the gain mode", result);
            return false;
        }
        if (gainTenthsDb && !m_gains.empty()) {
            // The nearest gain the tuner supports.
            const int wanted = *gainTenthsDb;
            const int nearest = *std::min_element(
                m_gains.begin(), m_gains.end(),
                [wanted](int a, int b) { return std::abs(a - wanted) < std::abs(b - wanted); });
            result = rtlsdr_set_tuner_gain(m_device, nearest);
            if (result < 0) {
                error = failure("Setting the gain", result);
                return false;
            }
            gainTenthsDb = nearest;
        }
    }
    const std::scoped_lock lock(m_settingsMutex);
    m_settings.gainTenthsDb = gainTenthsDb;
    return true;
}

bool RtlSdrSource::setPpm(int ppm, std::string& error) {
    if (m_device != nullptr) {
        // librtlsdr returns −2 when the correction is already this value.
        const int result = rtlsdr_set_freq_correction(m_device, ppm);
        if (result < 0 && result != -2) {
            error = failure("Setting the frequency correction", result);
            return false;
        }
    }
    const std::scoped_lock lock(m_settingsMutex);
    m_settings.ppm = ppm;
    return true;
}

bool RtlSdrSource::setBiasTee(bool on, std::string& error) {
    if (m_device == nullptr) {
        error = "the receiver isn't open";
        return false;
    }
    const int result = rtlsdr_set_bias_tee(m_device, on ? 1 : 0);
    if (result < 0) {
        error = failure("Switching the bias-tee", result);
        return false;
    }
    m_biasTee.store(on);
    return true;
}

}  // namespace zarescope::device
