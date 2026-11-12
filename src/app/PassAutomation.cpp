// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/PassAutomation.h"

#include "device/IqMetadata.h"

#include <cmath>

namespace zarescope::app {

namespace {

// A file can't be retuned; it counts as "on frequency" this close to the
// downlink (Doppler at 137 MHz is about ±3.5 kHz).
constexpr int64_t kFileToleranceHz = 10'000;

double seconds(sat::Clock::duration d) {
    return std::chrono::duration<double>(d).count();
}

}  // namespace

PassAutomation::PassAutomation(device::Receiver& receiver, std::vector<sat::Satellite> catalog,
                               AutomationConfig config)
    : m_receiver(receiver),
      m_scheduler(std::move(catalog)),
      m_config(std::move(config)),
      m_runner(std::make_unique<integrations::SatDumpRunner>(m_config.satDumpCandidates)) {}

PassAutomation::~PassAutomation() {
    // Stop SatDump first (its callbacks write into m_result), then leave the
    // receiver as the user had it if a pass was in progress.
    m_runner.reset();
    if (m_phase == AutomationPhase::Preparing || m_phase == AutomationPhase::Recording ||
        m_phase == AutomationPhase::Finishing) {
        m_receiver.stopRecording();
        restoreReceiver();
    }
}

void PassAutomation::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled && (m_phase == AutomationPhase::Preparing || m_phase == AutomationPhase::Recording)) {
        stopCurrent();
    }
}

void PassAutomation::stopCurrent() { m_stopRequested = true; }

void PassAutomation::checkSatDump() { m_runner->probe(); }

void PassAutomation::addSimulatedPass(sat::PlannedPass pass) {
    m_simulated.push_back(std::move(pass));
}

AutomationStatus PassAutomation::status() const {
    AutomationStatus status;
    status.enabled = m_enabled;
    status.phase = (!m_enabled && m_phase == AutomationPhase::Waiting) ? AutomationPhase::Off
                                                                       : m_phase;
    status.current = m_plan;
    status.next = m_next;
    status.bytesRecorded = m_bytes;
    status.satdump = m_runner->status();
    status.last = m_last;
    return status;
}

void PassAutomation::update(sat::Clock::time_point now,
                            const std::vector<sat::PlannedPass>& passes) {
    const device::ReceiverStatus receiver = m_receiver.status();
    syncArbiter(receiver);
    collectDecodeResult();

    std::vector<sat::PlannedPass> all = passes;
    all.insert(all.end(), m_simulated.begin(), m_simulated.end());
    m_next = m_enabled ? m_scheduler.next(now, all) : std::nullopt;

    switch (m_phase) {
        case AutomationPhase::Off:
        case AutomationPhase::Waiting:
            if (m_enabled) {
                if (auto plan = m_scheduler.due(now, all)) {
                    begin(*plan, receiver, now);
                }
            }
            break;
        case AutomationPhase::Preparing: prepare(receiver, now); break;
        case AutomationPhase::Recording: record(now); break;
        case AutomationPhase::Finishing: finish(now); break;
        case AutomationPhase::Decoding: break;  // collectDecodeResult() ends it
    }
}

void PassAutomation::syncArbiter(const device::ReceiverStatus& receiver) {
    // Follow what the user opens and closes; during a pass the arbiter
    // ignores these (the pass owns the stick).
    if (receiver.state == device::ReceiverState::Running) {
        m_arbiter.sourceOpened();
    } else if (receiver.state == device::ReceiverState::Closed ||
               receiver.state == device::ReceiverState::Failed) {
        m_arbiter.sourceClosed();
    }
}

bool PassAutomation::covers(const device::ReceiverStatus& receiver) const {
    if (receiver.state != device::ReceiverState::Running || !m_plan) {
        return false;
    }
    if (receiver.isDevice) {
        return receiver.source.centerFrequencyHz == m_plan->frequencyHz &&
               receiver.source.sampleRateSps == m_plan->sampleRateSps;
    }
    return std::llabs(receiver.source.centerFrequencyHz - m_plan->frequencyHz) <=
           kFileToleranceHz;
}

void PassAutomation::begin(const sat::RecordingPlan& plan, const device::ReceiverStatus& receiver,
                           sat::Clock::time_point now) {
    if (!m_arbiter.claimForPass()) {
        return;  // the stick is with another process; try again next frame
    }
    m_plan = plan;
    m_phase = AutomationPhase::Preparing;
    m_phaseStart = now;
    m_stopRequested = false;
    m_bytes = 0;
    m_recordingPath.clear();
    m_restore = {};
    if (receiver.state == device::ReceiverState::Running && receiver.isDevice) {
        m_restore = {Restore::Kind::Retune, receiver.source.centerFrequencyHz,
                     receiver.source.sampleRateSps};
        m_receiver.setSampleRate(plan.sampleRateSps);
        m_receiver.setCenterFrequency(plan.frequencyHz);
    } else if (receiver.state == device::ReceiverState::Running) {
        // A recording is playing: it can't be retuned (see covers()).
    } else {
        // Nothing open (or it failed): open the first stick, already tuned.
        m_restore.kind = Restore::Kind::Close;
        device::RtlSdrSettings settings;
        settings.centerFrequencyHz = plan.frequencyHz;
        settings.sampleRateSps = plan.sampleRateSps;
        settings.ppm = m_config.ppm;
        m_receiver.openDevice(0, settings);
    }
}

void PassAutomation::prepare(const device::ReceiverStatus& receiver, sat::Clock::time_point now) {
    if (m_stopRequested || now >= m_plan->pass.pass.los) {
        giveUp(m_stopRequested ? PassFailure::Stopped : PassFailure::PassEnded,
               m_stopRequested ? "stopped before recording began" : "the pass ended");
        return;
    }
    if (!covers(receiver)) {
        if (receiver.state == device::ReceiverState::Running && !receiver.isDevice) {
            giveUp(PassFailure::OffFrequency,
                   "the recording being played isn't on the satellite's frequency");
        } else if (receiver.state == device::ReceiverState::Failed) {
            giveUp(PassFailure::ReceiverFailed,
                   receiver.error.empty() ? "the receiver failed" : receiver.error);
        } else if (now - m_phaseStart > kPrepareTimeout) {
            giveUp(PassFailure::TuneTimeout, "the receiver didn't tune in time");
        }
        return;
    }
    // A recording the user had running ends here: the pass needs the recorder.
    if (m_receiver.recording().state != device::RecorderState::Idle) {
        m_receiver.stopRecording();
        return;
    }
    const double left = seconds(m_plan->pass.pass.los - now);
    const auto expectedBytes =
        static_cast<uint64_t>(left * receiver.source.sampleRateSps * 2.0);  // 2 bytes per sample
    if (m_receiver.startRecording(m_config.recordings, m_config.software, expectedBytes)) {
        m_phase = AutomationPhase::Recording;
        m_phaseStart = now;
        m_recordingStart = now;
        m_recordingPath = m_receiver.recording().path;
        m_recordRateSps = receiver.source.sampleRateSps;  // a played file's own rate
    } else if (now - m_phaseStart > kPrepareTimeout) {
        giveUp(PassFailure::RecorderFailed, "the recorder didn't start");
    }
}

void PassAutomation::record(sat::Clock::time_point now) {
    const device::RecorderStatus recording = m_receiver.recording();
    m_bytes = recording.bytesWritten;
    if (recording.state == device::RecorderState::Idle) {
        // Ended by itself: a disk error, or the source went away.
        m_phase = AutomationPhase::Finishing;
        return;
    }
    if (m_stopRequested || now >= m_plan->pass.pass.los) {
        m_receiver.stopRecording();
        m_phase = AutomationPhase::Finishing;
        m_phaseStart = now;
    }
}

void PassAutomation::finish(sat::Clock::time_point now) {
    const device::RecorderStatus recording = m_receiver.recording();
    if (recording.state != device::RecorderState::Idle) {
        return;  // still flushing
    }
    m_bytes = recording.bytesWritten;
    restoreReceiver();
    m_arbiter.releasePass();
    const sat::RecordingPlan plan = *m_plan;
    m_scheduler.markDone(plan.pass);

    PassOutcome outcome;
    outcome.satellite = plan.pass.satellite;
    outcome.aos = plan.pass.pass.aos;
    const double recordedSeconds =
        static_cast<double>(m_bytes) / 2.0 / std::max(1.0, static_cast<double>(m_recordRateSps));
    if (!recording.error.empty() && m_bytes == 0) {
        outcome.failure = PassFailure::RecorderFailed;
        outcome.error = recording.error;
    } else if (recordedSeconds < static_cast<double>(m_config.minRecording.count())) {
        outcome.failure =
            recording.error.empty() ? PassFailure::TooShort : PassFailure::RecorderFailed;
        outcome.error = recording.error.empty() ? "the recording is too short to decode"
                                                : recording.error;
    }
    if (outcome.failure != PassFailure::None) {
        m_phase = AutomationPhase::Waiting;
        m_plan.reset();
        setOutcome(std::move(outcome));
        return;
    }

    integrations::SatDumpJob job;
    job.baseband = m_recordingPath;
    job.productsDir =
        m_config.products / sat::productFolderName(plan.pass.pass.aos, plan.pass.satellite);
    job.pipeline = plan.pipeline;
    job.sampleRateSps = m_recordRateSps;
    job.extraArgs = plan.satdumpArgs;
    job.keepBaseband = m_config.keepBaseband;
    job.passInfo["satellite"] = plan.pass.satellite;
    job.passInfo["noradId"] = plan.pass.noradId;
    job.passInfo["aosUtc"] = device::formatUtcTimestamp(plan.pass.pass.aos);
    job.passInfo["losUtc"] = device::formatUtcTimestamp(plan.pass.pass.los);
    job.passInfo["maxElevationDeg"] = plan.pass.pass.maxElevationDeg;
    job.passInfo["frequencyHz"] = plan.frequencyHz;
    job.passInfo["sampleRateSps"] = job.sampleRateSps;
    job.passInfo["recordedFromUtc"] = device::formatUtcTimestamp(m_recordingStart);
    job.passInfo["recordedUntilUtc"] = device::formatUtcTimestamp(now);
    job.passInfo["recordingBytes"] = m_bytes;
    job.passInfo["stoppedByUser"] = m_stopRequested;
    job.passInfo["recording"] = m_recordingPath.filename().string();

    outcome.folder = job.productsDir;
    m_phase = AutomationPhase::Decoding;
    m_runner->submit(std::move(job),
                     [this, outcome](const integrations::SatDumpResult& result) {
                         const std::scoped_lock lock(m_resultMutex);
                         m_result.emplace(outcome, result);
                     });
}

void PassAutomation::collectDecodeResult() {
    std::optional<std::pair<PassOutcome, integrations::SatDumpResult>> result;
    {
        const std::scoped_lock lock(m_resultMutex);
        result.swap(m_result);
    }
    if (!result) {
        return;
    }
    auto& [outcome, decoded] = *result;
    outcome.ok = decoded.ok;
    outcome.images = decoded.images.size();
    outcome.log = decoded.log;
    outcome.error = decoded.error;
    switch (decoded.failure) {
        case integrations::SatDumpFailure::None: outcome.failure = PassFailure::None; break;
        case integrations::SatDumpFailure::NotInstalled:
            outcome.failure = PassFailure::SatDumpMissing;
            break;
        case integrations::SatDumpFailure::NoImage: outcome.failure = PassFailure::NoImage; break;
        case integrations::SatDumpFailure::CouldNotRun:
        case integrations::SatDumpFailure::TimedOut:
        case integrations::SatDumpFailure::Cancelled:
        case integrations::SatDumpFailure::Failed:
            outcome.failure = PassFailure::SatDumpFailed;
            break;
    }
    if (m_phase == AutomationPhase::Decoding) {
        m_phase = AutomationPhase::Waiting;
        m_plan.reset();
    }
    setOutcome(std::move(outcome));
}

void PassAutomation::giveUp(PassFailure failure, std::string error) {
    restoreReceiver();
    m_arbiter.releasePass();
    PassOutcome outcome;
    outcome.satellite = m_plan->pass.satellite;
    outcome.aos = m_plan->pass.pass.aos;
    outcome.failure = failure;
    outcome.error = std::move(error);
    m_scheduler.markDone(m_plan->pass);
    m_plan.reset();
    m_phase = AutomationPhase::Waiting;
    setOutcome(std::move(outcome));
}

void PassAutomation::restoreReceiver() {
    switch (m_restore.kind) {
        case Restore::Kind::Nothing: break;
        case Restore::Kind::Close: m_receiver.close(); break;
        case Restore::Kind::Retune:
            m_receiver.setSampleRate(m_restore.sampleRateSps);
            m_receiver.setCenterFrequency(m_restore.frequencyHz);
            break;
    }
    m_restore = {};
}

void PassAutomation::setOutcome(PassOutcome outcome) {
    outcome.serial = ++m_serial;
    m_last = std::move(outcome);
}

}  // namespace zarescope::app
