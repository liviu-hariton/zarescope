// SPDX-License-Identifier: GPL-3.0-or-later
//
// Receiver: the receive chain, from a sample source to what the UI draws.
//
//   source thread ──blocks──▶ DSP thread ──rows──▶ UI (waterfall)
//   (stick or file)  SpscSlots   │  └─trace──▶ UI (spectrum line, latest only)
//                                └─offer──▶ IqRecorder (its own writer thread)
//
//   control thread: opens and closes sources and runs every librtlsdr control
//   call (tuning, gain…), which can take tens of milliseconds over USB.
//
// The UI thread only posts commands (they return at once) and reads
// results; it never waits on the device, the DSP or the disk. This is the
// architecture in docs/ARCHITECTURE.md, as far as M1 needs it.

#pragma once

#include "audio/AudioOutput.h"
#include "decoders/adsb/Demodulator.h"
#include "device/FileSource.h"
#include "device/IqRecorder.h"
#include "device/RtlSdrSource.h"
#include "device/SampleSource.h"
#include "dsp/AudioDemodulator.h"
#include "dsp/SpscSlots.h"
#include "dsp/TripleBuffer.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace zarescope::device {

inline constexpr std::array<size_t, 5> kFftSizes{1024, 2048, 4096, 8192, 16384};
inline constexpr size_t kMaxFftSize = 16384;
inline constexpr size_t kDefaultFftSize = 4096;

// One spectrum row: a waterfall line, or the smoothed trace.
struct SpectrumRow {
    std::vector<float> dbfs;  // FFT-shifted, −fs/2 … +fs/2; capacity kMaxFftSize
    int64_t centerFrequencyHz = 0;
    uint32_t sampleRateSps = 0;
    std::chrono::system_clock::time_point timeUtc;
};

enum class ReceiverState { Closed, Opening, Running, Failed };

struct ReceiverStatus {
    ReceiverState state = ReceiverState::Closed;
    SourceInfo source;  // valid while Running
    std::string error;  // why it failed, or the last control that failed
    // Device sources only:
    bool isDevice = false;
    RtlSdrDeviceEntry device;
    DeviceIdentity identity;
    std::vector<int> gainsTenthsDb;
    RtlSdrSettings deviceSettings;
    bool biasTee = false;
    // File sources only:
    std::filesystem::path file;
    // Health:
    uint64_t droppedBlocks = 0;  // the DSP thread fell behind the source
    uint64_t droppedRows = 0;    // the UI fell behind the DSP thread
    size_t fftSize = kDefaultFftSize;
    // Listening (M2):
    float channelPowerDb = -200.0F;  // power in the demodulated channel
    bool squelchOpen = false;
    uint32_t audioRateHz = 0;  // 0 until the sound output runs
    std::string audioError;    // why there is no sound, if so
    // ADS-B (M3): decoding runs whenever the source is at 2 MS/s with
    // 1090 MHz near the centre.
    bool adsbActive = false;
    uint64_t adsbMessages = 0;
    uint64_t adsbRejected = 0;
};

// One decoded Mode S message and when it arrived.
struct AdsbMessage {
    adsb::RawMessage message;
    std::chrono::system_clock::time_point time;
};

class Receiver {
public:
    Receiver();
    ~Receiver();
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;
    Receiver(Receiver&&) = delete;
    Receiver& operator=(Receiver&&) = delete;

    // --- Commands (UI thread; all return at once) ---------------------------
    void scanDevices();
    void openDevice(uint32_t index, RtlSdrSettings settings);
    // `options` is for tests; the app plays files in real time, looping.
    void openFile(std::filesystem::path path, FileSourceOptions options = {});
    void close();
    void setCenterFrequency(int64_t frequencyHz);   // devices only
    void setSampleRate(uint32_t sampleRateSps);     // devices only
    void setGain(std::optional<int> gainTenthsDb);  // devices only
    void setPpm(int ppm);                           // devices only
    void setBiasTee(bool on);                       // devices only
    void setFftSize(size_t fftSize);                // one of kFftSizes
    // Weight of each new row in the spectrum trace: 1 = no smoothing.
    void setTraceSmoothing(float weight);
    // What to listen to: mode, offset from the centre, bandwidth, squelch,
    // volume. Cheap to call every frame; the DSP thread picks up changes
    // between blocks. Opens the sound output the first time a mode is set.
    void setDemod(const dsp::DemodSettings& settings);

    // Starts recording what is being received into `folder`. False when
    // nothing is running or a recording is still finishing.
    bool startRecording(const std::filesystem::path& folder, const std::string& software);
    void stopRecording();
    [[nodiscard]] RecorderStatus recording() const { return m_recorder.status(); }

    // --- Results (UI thread) -------------------------------------------------
    [[nodiscard]] ReceiverStatus status() const;
    [[nodiscard]] std::vector<RtlSdrDeviceEntry> devices() const;
    [[nodiscard]] bool scanning() const { return m_scanning.load(); }

    // The newest smoothed spectrum, if there is one newer than last time.
    bool latestTrace(SpectrumRow& out);
    // Calls `visit` for every waterfall row produced since the last call,
    // oldest first. Returns how many there were.
    size_t drainRows(const std::function<void(const SpectrumRow&)>& visit);

    // Calls `visit` for every ADS-B message decoded since the last call.
    size_t drainAdsb(const std::function<void(const AdsbMessage&)>& visit);

    // For tests: blocks until the control thread has run every command
    // posted so far.
    void waitForCommands();

private:
    void post(std::function<void()> command);
    void controlLoop();
    void dspLoop();
    void startStreaming(std::unique_ptr<SampleSource> source);
    void stopStreaming();
    void setError(std::string error);
    template <typename Control>
    void deviceControl(Control&& control);

    // Command queue, control thread.
    std::mutex m_commandMutex;
    std::condition_variable m_commandSignal;
    std::deque<std::function<void()>> m_commands;
    bool m_quit = false;
    size_t m_commandsDone = 0;    // guarded by m_commandMutex
    size_t m_commandsPosted = 0;  // guarded by m_commandMutex
    std::condition_variable m_commandsDoneSignal;
    std::thread m_control;

    // Owned by the control thread.
    std::unique_ptr<SampleSource> m_source;
    RtlSdrSource* m_device = nullptr;  // m_source, when it is a stick

    // Source → DSP.
    BlockQueue m_blocks;
    std::thread m_dsp;
    std::atomic<bool> m_dspRunning{false};

    // Coalesced tuning (see setCenterFrequency).
    std::atomic<int64_t> m_pendingFrequencyHz{0};
    std::atomic<bool> m_tunePending{false};

    // DSP settings, read by the DSP thread.
    std::atomic<size_t> m_fftSize{kDefaultFftSize};
    std::atomic<float> m_traceWeight{0.4F};

    // DSP → UI.
    dsp::SpscSlots<SpectrumRow> m_rows;
    dsp::TripleBuffer<SpectrumRow> m_trace;
    std::atomic<uint64_t> m_droppedRows{0};

    IqRecorder m_recorder;

    // Listening: settings UI → DSP (latest wins), the sound output (created
    // and started on the control thread; its ring is filled by the DSP
    // thread once m_audioRate is non-zero), and what the DSP reports back.
    dsp::TripleBuffer<dsp::DemodSettings> m_demodSettings;
    dsp::DemodSettings m_lastDemod;  // UI thread: what was last published
    bool m_audioRequested = false;   // UI thread
    std::unique_ptr<audio::AudioOutput> m_audio;
    std::atomic<uint32_t> m_audioRate{0};
    std::atomic<float> m_channelPowerDb{-200.0F};

    // ADS-B: DSP thread → UI.
    dsp::SpscSlots<AdsbMessage> m_adsb;
    std::atomic<bool> m_adsbActive{false};
    std::atomic<uint64_t> m_adsbMessages{0};
    std::atomic<uint64_t> m_adsbRejected{0};
    std::atomic<bool> m_squelchOpen{false};

    // Status, written by the control thread.
    mutable std::mutex m_statusMutex;
    ReceiverStatus m_status;
    std::vector<RtlSdrDeviceEntry> m_devices;
    std::atomic<bool> m_scanning{false};
};

}  // namespace zarescope::device
