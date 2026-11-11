// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/Receiver.h"

#include "device/FileSource.h"
#include "dsp/SpectrumAnalyzer.h"

#include <algorithm>

namespace zarescope::device {

namespace {

constexpr size_t kRowSlots = 64;  // ~2 s of waterfall at 30 rows/s
constexpr auto kControlTick = std::chrono::milliseconds(100);
constexpr auto kDspIdle = std::chrono::milliseconds(1);

void reserveRow(SpectrumRow& row) { row.dbfs.reserve(kMaxFftSize); }

}  // namespace

Receiver::Receiver()
    : m_blocks(makeBlockQueue()), m_rows(kRowSlots, reserveRow), m_trace(reserveRow) {
    m_control = std::thread([this] { controlLoop(); });
}

Receiver::~Receiver() {
    {
        const std::scoped_lock lock(m_commandMutex);
        m_quit = true;
    }
    m_commandSignal.notify_all();
    m_control.join();
}

// --- Commands -----------------------------------------------------------------

void Receiver::post(std::function<void()> command) {
    {
        const std::scoped_lock lock(m_commandMutex);
        m_commands.push_back(std::move(command));
        ++m_commandsPosted;
    }
    m_commandSignal.notify_one();
}

void Receiver::waitForCommands() {
    std::unique_lock lock(m_commandMutex);
    const size_t target = m_commandsPosted;
    m_commandsDoneSignal.wait(lock, [&] { return m_commandsDone >= target; });
}

void Receiver::scanDevices() {
    m_scanning.store(true);
    post([this] {
        auto devices = listRtlSdrDevices();
        {
            const std::scoped_lock lock(m_statusMutex);
            m_devices = std::move(devices);
        }
        m_scanning.store(false);
    });
}

void Receiver::openDevice(uint32_t index, RtlSdrSettings settings) {
    {
        const std::scoped_lock lock(m_statusMutex);
        m_status.state = ReceiverState::Opening;
        m_status.error.clear();
    }
    post([this, index, settings] {
        stopStreaming();
        auto source = std::make_unique<RtlSdrSource>(index, settings);
        m_device = source.get();
        startStreaming(std::move(source));
    });
}

void Receiver::openFile(std::filesystem::path path, FileSourceOptions options) {
    {
        const std::scoped_lock lock(m_statusMutex);
        m_status.state = ReceiverState::Opening;
        m_status.error.clear();
    }
    post([this, path = std::move(path), options] {
        stopStreaming();
        try {
            startStreaming(std::make_unique<FileSource>(path, options));
        } catch (const std::exception& e) {
            setError(e.what());
        }
    });
}

void Receiver::close() {
    post([this] {
        stopStreaming();
        const std::scoped_lock lock(m_statusMutex);
        m_status = ReceiverStatus{};
        m_status.fftSize = m_fftSize.load();
    });
}

template <typename Control>
void Receiver::deviceControl(Control&& control) {
    post([this, control = std::forward<Control>(control)] {
        if (m_device == nullptr) {
            return;  // files can't be tuned; the UI disables these controls
        }
        std::string error;
        if (!control(*m_device, error)) {
            const std::scoped_lock lock(m_statusMutex);
            m_status.error = error;
        }
        const std::scoped_lock lock(m_statusMutex);
        m_status.deviceSettings = m_device->settings();
        m_status.biasTee = m_device->biasTee();
        m_status.source = m_device->info();
    });
}

void Receiver::setCenterFrequency(int64_t frequencyHz) {
    deviceControl([frequencyHz](RtlSdrSource& device, std::string& error) {
        return device.setCenterFrequency(frequencyHz, error);
    });
}

void Receiver::setSampleRate(uint32_t sampleRateSps) {
    deviceControl([sampleRateSps](RtlSdrSource& device, std::string& error) {
        return device.setSampleRate(sampleRateSps, error);
    });
}

void Receiver::setGain(std::optional<int> gainTenthsDb) {
    deviceControl([gainTenthsDb](RtlSdrSource& device, std::string& error) {
        return device.setGain(gainTenthsDb, error);
    });
}

void Receiver::setPpm(int ppm) {
    deviceControl(
        [ppm](RtlSdrSource& device, std::string& error) { return device.setPpm(ppm, error); });
}

void Receiver::setBiasTee(bool on) {
    deviceControl(
        [on](RtlSdrSource& device, std::string& error) { return device.setBiasTee(on, error); });
}

void Receiver::setFftSize(size_t fftSize) {
    if (std::find(kFftSizes.begin(), kFftSizes.end(), fftSize) == kFftSizes.end()) {
        return;
    }
    m_fftSize.store(fftSize);
    const std::scoped_lock lock(m_statusMutex);
    m_status.fftSize = fftSize;
}

void Receiver::setTraceSmoothing(float weight) {
    m_traceWeight.store(std::clamp(weight, 0.01F, 1.0F));
}

bool Receiver::startRecording(const std::filesystem::path& folder, const std::string& software) {
    const ReceiverStatus now = status();
    if (now.state != ReceiverState::Running) {
        return false;
    }
    IqMetadata metadata;
    metadata.centerFrequencyHz = now.source.centerFrequencyHz;
    metadata.sampleRateSps = now.source.sampleRateSps;
    metadata.startUtc = std::chrono::system_clock::now();
    metadata.software = software;
    if (now.isDevice) {
        metadata.gainTenthsDb = now.deviceSettings.gainTenthsDb;
        metadata.ppm = now.deviceSettings.ppm;
        std::string device(modelName(now.identity.claimed));
        metadata.device = (device.empty() ? now.device.product : device) + " (" +
                          std::string(tunerName(now.identity.tuner)) + ")";
    } else {
        metadata.device = "recording " + now.file.filename().string();
    }
    return m_recorder.start(folder, std::move(metadata));
}

void Receiver::stopRecording() { m_recorder.requestStop(); }

// --- Results ------------------------------------------------------------------

ReceiverStatus Receiver::status() const {
    ReceiverStatus status;
    {
        const std::scoped_lock lock(m_statusMutex);
        status = m_status;
    }
    status.droppedRows = m_droppedRows.load(std::memory_order_relaxed);
    return status;
}

std::vector<RtlSdrDeviceEntry> Receiver::devices() const {
    const std::scoped_lock lock(m_statusMutex);
    return m_devices;
}

bool Receiver::latestTrace(SpectrumRow& out) {
    if (!m_trace.update()) {
        return false;
    }
    const SpectrumRow& trace = m_trace.readBuffer();
    out.dbfs.assign(trace.dbfs.begin(), trace.dbfs.end());
    out.centerFrequencyHz = trace.centerFrequencyHz;
    out.sampleRateSps = trace.sampleRateSps;
    out.timeUtc = trace.timeUtc;
    return true;
}

size_t Receiver::drainRows(const std::function<void(const SpectrumRow&)>& visit) {
    size_t count = 0;
    while (SpectrumRow* row = m_rows.beginRead()) {
        visit(*row);
        m_rows.commitRead();
        ++count;
    }
    return count;
}

// --- Control thread -----------------------------------------------------------

void Receiver::controlLoop() {
    std::unique_lock lock(m_commandMutex);
    while (!m_quit) {
        m_commandSignal.wait_for(lock, kControlTick,
                                 [this] { return m_quit || !m_commands.empty(); });
        while (!m_commands.empty()) {
            auto command = std::move(m_commands.front());
            m_commands.pop_front();
            lock.unlock();
            command();
            lock.lock();
            ++m_commandsDone;
        }
        m_commandsDoneSignal.notify_all();

        // Watch the running source: a stick that went away, a file that ended.
        lock.unlock();
        if (m_source && m_source->finished()) {
            const std::string error = m_source->lastError();
            stopStreaming();
            if (error.empty()) {
                const std::scoped_lock statusLock(m_statusMutex);
                m_status.state = ReceiverState::Closed;
            } else {
                setError(error);
            }
        } else if (m_source) {
            const std::scoped_lock statusLock(m_statusMutex);
            m_status.droppedBlocks = m_source->droppedBlocks();
        }
        lock.lock();
    }
    lock.unlock();
    stopStreaming();
}

void Receiver::startStreaming(std::unique_ptr<SampleSource> source) {
    std::string error;
    if (!source->start(m_blocks, error)) {
        m_device = nullptr;
        setError(error);
        return;
    }
    m_source = std::move(source);

    ReceiverStatus status;
    status.state = ReceiverState::Running;
    status.source = m_source->info();
    status.fftSize = m_fftSize.load();
    if (m_device != nullptr) {
        status.isDevice = true;
        status.device = m_device->entry();
        status.identity = m_device->identity();
        status.gainsTenthsDb = m_device->supportedGainsTenthsDb();
        status.deviceSettings = m_device->settings();
        status.biasTee = m_device->biasTee();
    } else if (auto* file = dynamic_cast<FileSource*>(m_source.get())) {
        status.file = file->path();
    }
    {
        const std::scoped_lock lock(m_statusMutex);
        m_status = std::move(status);
    }

    m_dspRunning.store(true);
    m_dsp = std::thread([this] { dspLoop(); });
}

void Receiver::stopStreaming() {
    m_recorder.requestStop();
    m_dspRunning.store(false);
    if (m_dsp.joinable()) {
        m_dsp.join();
    }
    if (m_source) {
        m_source->stop();
        m_source.reset();
    }
    m_device = nullptr;
    // With both threads stopped, this thread may act as the consumer.
    while (m_blocks.beginRead() != nullptr) {
        m_blocks.commitRead();
    }
}

void Receiver::setError(std::string error) {
    const std::scoped_lock lock(m_statusMutex);
    m_status.state = ReceiverState::Failed;
    m_status.error = std::move(error);
}

// --- DSP thread ---------------------------------------------------------------

void Receiver::dspLoop() {
    // Everything this thread needs is allocated here, before the loop, and
    // again only when the FFT size changes.
    size_t fftSize = m_fftSize.load();
    auto analyzer =
        std::make_unique<dsp::SpectrumAnalyzer>(dsp::SpectrumConfig{.fftSize = fftSize});
    std::vector<float> trace;
    trace.reserve(kMaxFftSize);
    int64_t center = 0;
    uint32_t rate = 0;
    std::chrono::system_clock::time_point blockTime;

    // Receives each finished row: queues it for the waterfall and folds it
    // into the smoothed trace.
    struct Sink final : dsp::SpectrumRowSink {
        Receiver& receiver;
        std::vector<float>& trace;
        const int64_t& center;
        const uint32_t& rate;
        const std::chrono::system_clock::time_point& time;
        Sink(Receiver& r, std::vector<float>& t, const int64_t& c, const uint32_t& s,
             const std::chrono::system_clock::time_point& tm)
            : receiver(r), trace(t), center(c), rate(s), time(tm) {}

        void onSpectrumRow(std::span<const float> dbfs) override {
            auto fill = [&](SpectrumRow& row, std::span<const float> values) {
                row.dbfs.assign(values.begin(), values.end());
                row.centerFrequencyHz = center;
                row.sampleRateSps = rate;
                row.timeUtc = time;
            };
            if (SpectrumRow* row = receiver.m_rows.beginWrite()) {
                fill(*row, dbfs);
                receiver.m_rows.commitWrite();
            } else {
                receiver.m_droppedRows.fetch_add(1, std::memory_order_relaxed);
            }
            // Exponential smoothing in dB: trace += w·(row − trace).
            const float weight = receiver.m_traceWeight.load(std::memory_order_relaxed);
            if (trace.size() != dbfs.size()) {
                trace.assign(dbfs.begin(), dbfs.end());
            } else {
                for (size_t i = 0; i < dbfs.size(); ++i) {
                    trace[i] += weight * (dbfs[i] - trace[i]);
                }
            }
            fill(receiver.m_trace.writeBuffer(), trace);
            receiver.m_trace.publish();
        }
    } sink(*this, trace, center, rate, blockTime);

    while (m_dspRunning.load(std::memory_order_acquire)) {
        if (m_fftSize.load(std::memory_order_relaxed) != fftSize) {
            fftSize = m_fftSize.load();
            analyzer =
                std::make_unique<dsp::SpectrumAnalyzer>(dsp::SpectrumConfig{.fftSize = fftSize});
            analyzer->setSampleRate(rate == 0 ? 2'400'000 : rate);
            trace.clear();
        }
        SampleBlock* block = m_blocks.beginRead();
        if (block == nullptr) {
            std::this_thread::sleep_for(kDspIdle);
            continue;
        }
        if (block->centerFrequencyHz != center || block->sampleRateSps != rate) {
            // Retuned: start the rows and the trace afresh.
            center = block->centerFrequencyHz;
            rate = block->sampleRateSps;
            analyzer->setSampleRate(rate);
            trace.clear();
        }
        blockTime = block->firstSampleUtc;
        m_recorder.offer(*block);
        analyzer->process(block->bytes, sink);
        m_blocks.commitRead();
    }
}

}  // namespace zarescope::device
