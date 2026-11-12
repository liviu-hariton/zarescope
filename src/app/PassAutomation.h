// SPDX-License-Identifier: GPL-3.0-or-later
//
// PassAutomation: records and decodes satellite passes by itself (M5).
//
// The pieces it ties together, each testable on its own:
//   sat::PassScheduler          which pass is due
//   device::DeviceArbiter       who owns the stick (the pass outranks the user)
//   device::Receiver            tuning and the IQ recorder
//   integrations::SatDumpRunner decoding after LOS, in another process
//
// One pass, phase by phase:
//
//   Waiting    nothing due (or automatic recording is off)
//   Preparing  the stick is claimed and tuned to the catalog frequency and
//              sample rate (opened first if nothing was open); waits until
//              the receiver reports exactly that
//   Recording  baseband from now until LOS, or until the user stops it
//   Finishing  the recorder flushes; the receiver goes back to what the user
//              had before (frequency and rate, or closed); the user gets the
//              controls back
//   Decoding   SatDump runs on the recording; its images land in
//              products/<date>_<satellite>/
//
// A recording that plays from a file (FileSource) can't be retuned: if it
// covers the downlink frequency, it is recorded as is. That, with a fake
// clock, is how a pass is simulated without a stick (tests and
// --simulate-pass).
//
// update() is called from the UI thread every frame and only posts
// commands: it never waits for the device, the disk or SatDump.

#pragma once

#include "device/DeviceArbiter.h"
#include "device/Receiver.h"
#include "integrations/SatDumpRunner.h"
#include "sat/PassScheduler.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace zarescope::app {

enum class AutomationPhase { Off, Waiting, Preparing, Recording, Finishing, Decoding };

// Why a pass produced no image; the UI words each one in both languages.
enum class PassFailure {
    None,
    Stopped,         // the user stopped it before recording began
    PassEnded,       // over before the receiver was ready
    OffFrequency,    // a played recording isn't on the downlink
    ReceiverFailed,  // no stick, or it failed (detail: why)
    TuneTimeout,     // the receiver didn't report the frequency in time
    RecorderFailed,  // disk full, no space… (detail: why)
    TooShort,        // too little was recorded to decode
    SatDumpMissing,
    SatDumpFailed,  // couldn't run, crashed, or timed out (detail: why)
    NoImage,        // SatDump ran but found no image: usually a weak signal
};

// How the last pass ended. `serial` grows by one per outcome, so the UI
// can tell a new one (and notify once).
struct PassOutcome {
    uint64_t serial = 0;
    std::string satellite;
    sat::Clock::time_point aos;
    bool ok = false;
    size_t images = 0;
    std::filesystem::path folder;  // products/<date>_<satellite>, when decoding ran
    std::filesystem::path log;
    PassFailure failure = PassFailure::None;
    std::string error;  // English detail, for the log and the banner's tail
};

struct AutomationStatus {
    bool enabled = false;
    AutomationPhase phase = AutomationPhase::Off;
    std::optional<sat::RecordingPlan> current;  // the pass being handled
    std::optional<sat::PlannedPass> next;       // the next one that will be
    uint64_t bytesRecorded = 0;
    integrations::SatDumpStatus satdump;
    std::optional<PassOutcome> last;
};

struct AutomationConfig {
    std::filesystem::path recordings;  // pass recordings (baseband .cu8)
    std::filesystem::path products;    // decoded images, one folder per pass
    std::vector<std::filesystem::path> satDumpCandidates;  // platform::satDumpCandidates()
    std::string software;  // written into the recording's sidecar
    // Less than this much recording isn't worth decoding.
    std::chrono::seconds minRecording{30};
    // Keep the baseband after a successful decode (it is ~2 MB/s).
    bool keepBaseband = false;
    // Frequency correction for a stick the automation opens itself.
    int ppm = 0;
};

class PassAutomation {
public:
    // How long tuning may take before the pass is given up.
    static constexpr auto kPrepareTimeout = std::chrono::seconds(20);

    PassAutomation(device::Receiver& receiver, std::vector<sat::Satellite> catalog,
                   AutomationConfig config);
    ~PassAutomation();
    PassAutomation(const PassAutomation&) = delete;
    PassAutomation& operator=(const PassAutomation&) = delete;
    PassAutomation(PassAutomation&&) = delete;
    PassAutomation& operator=(PassAutomation&&) = delete;

    // UI thread. Turning it off during a pass stops that pass (what was
    // recorded is still decoded).
    void setEnabled(bool enabled);
    // UI thread, every frame. `passes` is SatelliteService's list.
    void update(sat::Clock::time_point now, const std::vector<sat::PlannedPass>& passes);
    // UI thread: stop the pass in progress (decodes what was recorded).
    void stopCurrent();
    // Looks for SatDump again (after the user installed it).
    void checkSatDump();
    // Settings that can change while running (M6): the stick's frequency
    // correction when a pass opens it, and whether to keep the baseband.
    void setPpm(int ppm) { m_config.ppm = ppm; }
    void setKeepBaseband(bool keep) { m_config.keepBaseband = keep; }
    // A made-up pass, for trying the whole chain without waiting for a real
    // one (--simulate-pass). Joins the predicted passes.
    void addSimulatedPass(sat::PlannedPass pass);

    [[nodiscard]] AutomationStatus status() const;
    // False while a pass holds the stick: the user's tuning controls are locked.
    [[nodiscard]] bool userMayControl() const { return m_arbiter.userMayControl(); }

    // For tests: blocks until SatDump has finished every job.
    void waitForDecoding() { m_runner->waitIdle(); }

private:
    // What the receiver was doing before the pass, to go back to it.
    struct Restore {
        enum class Kind { Nothing, Close, Retune } kind = Kind::Nothing;
        int64_t frequencyHz = 0;
        uint32_t sampleRateSps = 0;
    };

    void syncArbiter(const device::ReceiverStatus& receiver);
    void begin(const sat::RecordingPlan& plan, const device::ReceiverStatus& receiver,
               sat::Clock::time_point now);
    void prepare(const device::ReceiverStatus& receiver, sat::Clock::time_point now);
    void record(sat::Clock::time_point now);
    void finish(sat::Clock::time_point now);
    void giveUp(PassFailure failure, std::string error);
    void restoreReceiver();
    void collectDecodeResult();
    void setOutcome(PassOutcome outcome);
    [[nodiscard]] bool covers(const device::ReceiverStatus& receiver) const;

    device::Receiver& m_receiver;
    sat::PassScheduler m_scheduler;
    AutomationConfig m_config;
    device::DeviceArbiter m_arbiter;

    // UI thread only.
    bool m_enabled = false;
    AutomationPhase m_phase = AutomationPhase::Waiting;
    std::optional<sat::RecordingPlan> m_plan;
    Restore m_restore;
    sat::Clock::time_point m_phaseStart;
    sat::Clock::time_point m_recordingStart;
    bool m_stopRequested = false;
    std::filesystem::path m_recordingPath;
    uint64_t m_bytes = 0;
    uint32_t m_recordRateSps = 0;
    std::vector<sat::PlannedPass> m_simulated;
    std::optional<sat::PlannedPass> m_next;
    std::optional<PassOutcome> m_last;
    uint64_t m_serial = 0;

    // Runner thread → UI thread: the decode that just finished.
    mutable std::mutex m_resultMutex;
    std::optional<std::pair<PassOutcome, integrations::SatDumpResult>> m_result;

    // Last member: destroyed first, so its final callbacks still find the
    // members above alive.
    std::unique_ptr<integrations::SatDumpRunner> m_runner;
};

}  // namespace zarescope::app
