// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main window. See MainWindow.h for the layout.
//
// The shell is a fixed layout drawn with ImDrawList at computed positions;
// the interactive parts use InvisibleButton (through UiKit), so they still
// take part in keyboard navigation. The popovers use ordinary ImGui widgets,
// styled by the theme. Every colour and size comes from the design tokens;
// the few lengths that aren't tokens are named constants citing the spec.

#include "ui/MainWindow.h"

#include "ui/Clock.h"
#include "ui/Format.h"
#include "ui/LogoMark.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <utility>

namespace zarescope::ui {

namespace {

using device::ReceiverState;
using device::RecorderState;

// DESIGN.md §6: the top bar shows the small symbol at 28 px.
constexpr float kTopBarLogoPx = 28.0F;
// The empty state's symbol, at the full drawing's native 64-px grid.
constexpr float kEmptyStateLogoPx = 64.0F;
// Body text measure: about 65 characters of 13-px Inter.
constexpr float kMeasurePx = 440.0F;
// Popover content width (text wraps inside it).
constexpr float kPopoverWidth = 360.0F;
// DESIGN.md §5: icons in buttons and banners are 16 px.
constexpr float kIconPx = 16.0F;
// When the VFO leaves the captured span, the stick retunes so the VFO sits
// this fraction of the span off centre: away from the RTL-SDR's DC spike.
constexpr double kOffCentreFraction = 1.0 / 8.0;

// Click snapping and scroll steps per mode: FM broadcast on a 50/100 kHz
// raster, narrowband FM on 6.25/12.5 kHz, AM airband on 8.33 kHz channels.
int64_t snapFor(dsp::DemodMode mode) {
    switch (mode) {
    case dsp::DemodMode::Wfm: return 50'000;
    case dsp::DemodMode::Nfm: return 6'250;
    case dsp::DemodMode::Am: return 1'000;
    case dsp::DemodMode::Off: break;
    }
    return 1'000;
}
int64_t stepFor(dsp::DemodMode mode) {
    switch (mode) {
    case dsp::DemodMode::Wfm: return 100'000;
    case dsp::DemodMode::Nfm: return 12'500;
    case dsp::DemodMode::Am: return 8'333;
    case dsp::DemodMode::Off: break;
    }
    return 100'000;
}

// A preset's name, from the string table.
std::optional<StringId> presetName(std::string_view id) {
    if (id == "fm-broadcast") {
        return StringId::PresetFmBroadcast;
    }
    if (id == "airband") {
        return StringId::PresetAirband;
    }
    if (id == "amateur-2m") {
        return StringId::PresetAmateur2m;
    }
    if (id == "pmr446") {
        return StringId::PresetPmr446;
    }
    if (id == "adsb") {
        return StringId::PresetAdsb;
    }
    if (id == "meteor") {
        return StringId::PresetMeteor;
    }
    return std::nullopt;
}

// Defaults for a freshly opened stick: the FM broadcast band, 2.4 MS/s
// (the M1 default), automatic gain.
constexpr int64_t kDefaultFrequencyHz = 100'000'000;
constexpr uint32_t kDefaultSampleRate = 2'400'000;

// Begins a full-width, square, borderless bar or area. The style is pushed
// only around BeginChild (where ImGui reads it), so it doesn't leak into
// tooltips and popups opened inside the area.
void beginArea(const char* id, float height, ImU32 background);

constexpr ImGuiWindowFlags kAreaFlags = ImGuiWindowFlags_NoScrollbar |
                                        ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoSavedSettings;

void beginArea(const char* id, float height, ImU32 background) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
    ImGui::BeginChild(id, ImVec2(0.0F, height), ImGuiChildFlags_None, kAreaFlags);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

std::string cstr(std::string_view text) { return std::string(text); }

}  // namespace

MainWindow::MainWindow(const theme::DesignTokens& tokens, const Typography& typography,
                       MainWindowServices services)
    : m_ui(tokens, typography), m_services(std::move(services)), m_spectrum(tokens) {
    m_services.receiver->setTraceSmoothing(m_traceSmoothing);
    m_aircraft.setStation(m_services.station);
    if (m_services.station) {
        m_stationLat = m_services.station->latDeg;
        m_stationLon = m_services.station->lonDeg;
    }
}

bool MainWindow::wantsContinuousFrames() const {
    return m_status.state == ReceiverState::Running || m_status.state == ReceiverState::Opening ||
           m_recording.state != RecorderState::Idle;
}

void MainWindow::pullData() {
    device::Receiver& receiver = *m_services.receiver;
    m_status = receiver.status();
    m_recording = receiver.recording();
    receiver.drainRows([this](const device::SpectrumRow& row) { m_spectrum.addRow(row); });
    if (receiver.latestTrace(m_trace)) {
        m_spectrum.setTrace(m_trace);
    }
    // ADS-B: fold the decoded messages into the aircraft store.
    // Aircraft age by arrival time (now), not by the samples' timestamps: a
    // replayed recording would otherwise expire everything on arrival. Live,
    // the two are the same.
    const auto now = adsb::Clock::now();
    receiver.drainAdsb(
        [this, now](const device::AdsbMessage& m) { m_aircraft.update(m.message, now); });
    m_aircraft.expire(now);
    // When the source becomes ready for ADS-B (the preset, or a 1090 MHz
    // recording), show the aircraft. Only on that change, never against
    // the user's own choice afterwards.
    if (m_status.adsbActive && !m_wasAdsbActive) {
        m_tab = Tab::Aircraft;
    }
    m_wasAdsbActive = m_status.adsbActive;

    // A newly opened source: the VFO starts on its centre frequency.
    const bool running = m_status.state == ReceiverState::Running;
    const std::string key =
        running ? m_status.source.name + (m_status.isDevice ? "#device" : "#file") : "";
    if (key != m_sourceKey) {
        m_sourceKey = key;
        m_vfoHz = m_status.source.centerFrequencyHz;
    }
    // The demodulator listens at the VFO's offset from the centre.
    if (running) {
        m_demod.offsetHz = static_cast<double>(m_vfoHz - m_status.source.centerFrequencyHz);
        receiver.setDemod(m_demod);
    }
}

Vfo MainWindow::vfo() const {
    return {m_vfoHz, m_demod.mode == dsp::DemodMode::Off ? 0.0 : m_demod.bandwidthHz,
            snapFor(m_demod.mode), stepFor(m_demod.mode)};
}

void MainWindow::tuneTo(int64_t frequencyHz) {
    const auto& source = m_status.source;
    const double half = source.sampleRateSps / 2.0;
    const double margin = std::max(
        m_demod.mode == dsp::DemodMode::Off ? 0.0 : m_demod.bandwidthHz / 2.0, half * 0.05);
    const auto center = static_cast<double>(source.centerFrequencyHz);
    const auto lowest = static_cast<int64_t>(center - half + margin);
    const auto highest = static_cast<int64_t>(center + half - margin);
    frequencyHz = std::clamp(frequencyHz, device::kMinFrequencyHz, device::kMaxFrequencyHz);
    if (m_status.isDevice) {
        if (frequencyHz < lowest || frequencyHz > highest) {
            const auto newCenter = std::clamp(
                frequencyHz + static_cast<int64_t>(source.sampleRateSps * kOffCentreFraction),
                device::kMinFrequencyHz, device::kMaxFrequencyHz);
            m_services.receiver->setCenterFrequency(newCenter);
        }
        m_vfoHz = frequencyHz;
    } else {
        // A recording can't retune: the VFO stays inside what was captured.
        m_vfoHz = std::clamp(frequencyHz, lowest, std::max(lowest, highest));
    }
}

void MainWindow::setMode(dsp::DemodMode mode) {
    if (mode == m_demod.mode) {
        return;
    }
    m_demod.mode = mode;
    m_demod.bandwidthHz = dsp::defaultBandwidthHz(mode);
}

void MainWindow::applyPreset(const device::Preset& preset) {
    if (preset.sampleRateSps != 0 && m_status.isDevice &&
        preset.sampleRateSps != m_status.deviceSettings.sampleRateSps) {
        m_services.receiver->setSampleRate(preset.sampleRateSps);
    }
    setMode(preset.mode);
    if (preset.bandwidthHz > 0.0) {
        m_demod.bandwidthHz = preset.bandwidthHz;
    }
    tuneTo(preset.frequencyHz);
}

void MainWindow::draw(ShellState& state) {
    m_ui.beginFrame(state.variant, state.language);
    pullData();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0F, 0.0F));
    constexpr ImGuiWindowFlags kShellFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;
    ImGui::Begin("##shell", nullptr, kShellFlags);
    ImGui::PopStyleVar(3);

    drawTopBar(state);
    drawBody();
    drawStatusBar();

    // Popovers, at window level so any bar can open them.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(m_ui.spacing("16"), m_ui.spacing("16")));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(m_ui.spacing("8"), m_ui.spacing("8")));
    drawDevicePopup();
    drawGainPopup();
    drawFftPopup();
    drawAudioPopup();
    drawPresetsPopup();
    drawSettingsPopup(state);
    ImGui::PopStyleVar(2);

    ImGui::End();
}

// --- Top bar ----------------------------------------------------------------------

void MainWindow::drawTopBar(ShellState& /*state*/) {
    const float height = m_ui.size("topBar");
    beginArea("##topbar", height, m_ui.color("bg/surface"));

    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    const float pad = m_ui.spacing("16");
    const float gap = m_ui.spacing("8");
    const float group = m_ui.spacing("24");
    const float controlY = origin.y + (height - m_ui.size("control")) * 0.5F;
    const float chipY = origin.y + (height - m_ui.size("controlCompact")) * 0.5F;
    const bool running = m_status.state == ReceiverState::Running;
    const bool isDevice = running && m_status.isDevice;

    // Left: logo, frequency (the VFO) with its presets, gain, mode, sound.
    drawLogoSymbol(drawList, ImVec2(origin.x + pad, origin.y + (height - kTopBarLogoPx) * 0.5F),
                   kTopBarLogoPx,
                   {m_ui.logoColor("rings"), m_ui.logoColor("pass"), m_ui.color("bg/surface")});
    float x = origin.x + pad + kTopBarLogoPx + group;
    if (running) {
        const float freqY = origin.y + (height - m_frequency.height(m_ui)) * 0.5F;
        if (auto tuned = m_frequency.draw(m_ui, ImVec2(x, freqY), m_vfoHz, true,
                                          device::kMinFrequencyHz, device::kMaxFrequencyHz)) {
            tuneTo(*tuned);
        }
        x += m_frequency.width(m_ui, m_vfoHz) + gap;
        if (m_ui.iconButton("##presets", ImVec2(x, controlY), Icon::ChevronDown,
                            m_ui.tr(StringId::PresetsLabel))) {
            m_openPresets = true;
            m_presetsAnchor = ImVec2(x, origin.y + height);
        }
        x += m_ui.iconButtonWidth() + group;
    }
    if (isDevice) {
        const auto& gain = m_status.deviceSettings.gainTenthsDb;
        const std::string label = cstr(m_ui.tr(StringId::GainLabel)) + " " +
                                  (gain ? formatGain(*gain) : cstr(m_ui.tr(StringId::GainAuto)));
        if (m_ui.button("##gain", ImVec2(x, controlY), label, ButtonKind::Secondary,
                        Icon::ChevronDown)) {
            m_openGain = true;
            m_gainAnchor = ImVec2(x, origin.y + height);
        }
        x += m_ui.buttonWidth(label, Icon::ChevronDown) + gap;
    }
    if (running) {
        const std::array<std::string_view, 4> modes{m_ui.tr(StringId::ModeOff), "WFM", "NFM", "AM"};
        int mode = static_cast<int>(m_demod.mode);
        if (m_ui.segmented("mode", ImVec2(x, controlY), modes.data(), 4, mode,
                           m_ui.tr(StringId::ModeLabel))) {
            setMode(static_cast<dsp::DemodMode>(mode));
        }
        x += m_ui.segmentedWidth(modes.data(), 4) + gap;
        if (m_ui.iconButton("##audio", ImVec2(x, controlY), Icon::Speaker,
                            m_ui.tr(StringId::AudioLabel))) {
            m_openAudio = true;
            m_audioAnchor = ImVec2(x, origin.y + height);
        }
    }

    // Right, from the edge inwards: settings, record, live chip, device chip.
    float right = origin.x + width - pad - m_ui.iconButtonWidth();
    if (m_ui.iconButton("##settings", ImVec2(right, controlY), Icon::Settings,
                        m_ui.tr(StringId::SettingsLabel))) {
        m_openSettings = true;
        m_settingsAnchor = ImVec2(right + m_ui.iconButtonWidth(), origin.y + height);
    }
    if (running) {
        const bool recording = m_recording.state != RecorderState::Idle;
        const std::string_view label =
            recording ? m_ui.tr(StringId::StopRecording) : m_ui.tr(StringId::Record);
        const Icon icon = recording ? Icon::Stop : Icon::Record;
        right -= gap + m_ui.buttonWidth(label, icon);
        if (m_ui.button("##record", ImVec2(right, controlY), label, ButtonKind::Secondary, icon,
                        m_recording.state != RecorderState::Finishing)) {
            toggleRecording();
        }
        const std::string_view live =
            m_status.isDevice ? m_ui.tr(StringId::ChipLive) : m_ui.tr(StringId::ChipReplay);
        const Status liveStatus = m_status.isDevice ? Status::Live : Status::Neutral;
        right -= gap + m_ui.chipWidth(live, true);
        m_ui.chip("##live", ImVec2(right, chipY), live, liveStatus, false, true);
    }
    std::string deviceLabel = cstr(m_ui.tr(StringId::ReceiverChipNone));
    if (m_status.state == ReceiverState::Opening) {
        deviceLabel = cstr(m_ui.tr(StringId::ReceiverChipOpening));
    } else if (running) {
        deviceLabel = m_status.source.name;
    }
    right -= gap + m_ui.chipWidth(deviceLabel, false);
    if (m_ui.chip("##device", ImVec2(right, chipY), deviceLabel, Status::Neutral, true, false)) {
        m_openDevices = true;
    }
    m_deviceAnchor = ImVec2(right + m_ui.chipWidth(deviceLabel, false),
                            chipY + m_ui.size("controlCompact") + gap);

    drawList.AddRectFilled(ImVec2(origin.x, origin.y + height - m_ui.border()),
                           ImVec2(origin.x + width, origin.y + height),
                           m_ui.color("border/subtle"));
    ImGui::EndChild();
}

void MainWindow::toggleRecording() {
    device::Receiver& receiver = *m_services.receiver;
    if (m_recording.state == RecorderState::Recording) {
        receiver.stopRecording();
    } else {
        receiver.startRecording(m_services.recordingsFolder, "Zarescope " + m_services.versionText);
    }
}

// --- Banners ------------------------------------------------------------------------

// Draws at most one banner (recording first, then recording notices, then
// receiver errors) and returns its height, or 0.
float MainWindow::drawBanner(float top) {
    const ImVec2 min(ImGui::GetWindowPos().x, top);
    const float width = ImGui::GetWindowWidth();

    if (m_recording.state != RecorderState::Idle) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now() - m_recording.startUtc)
                                 .count();
        const std::string message =
            m_ui.trf(StringId::RecordingBanner, formatFrequency(m_status.source.centerFrequencyHz),
                     formatDuration(elapsed), formatBytes(m_recording.bytesWritten));
        if (m_ui.banner("##stopRecording", min, width, Status::Recording, Icon::Record, "REC",
                        message, m_ui.tr(StringId::StopRecording))) {
            m_services.receiver->stopRecording();
        }
        return m_ui.size("recordBanner");
    }
    const bool recordingNotice = (m_recording.endedByRetune || !m_recording.error.empty()) &&
                                 m_recording.path != m_dismissedRecording;
    if (recordingNotice) {
        const bool failed = !m_recording.error.empty();
        const std::string message =
            failed ? m_ui.trf(StringId::RecordingFailed, m_recording.error)
                   : m_ui.trf(StringId::RecordingEndedRetune, m_recording.path.filename().string());
        if (m_ui.banner("##dismissRecording", min, width, failed ? Status::Error : Status::Warning,
                        Icon::Alert, {}, message, m_ui.tr(StringId::Dismiss))) {
            m_dismissedRecording = m_recording.path;
        }
        return m_ui.size("recordBanner");
    }
    if (!m_status.error.empty() && m_status.error != m_dismissedError) {
        const std::string message = m_status.state == ReceiverState::Failed
                                        ? m_ui.trf(StringId::ReceiverFailed, m_status.error)
                                        : m_status.error;
        if (m_ui.banner("##dismissError", min, width, Status::Error, Icon::Alert, {}, message,
                        m_ui.tr(StringId::Dismiss))) {
            m_dismissedError = m_status.error;
        }
        return m_ui.size("recordBanner");
    }
    return 0.0F;
}

// --- Body ---------------------------------------------------------------------------

void MainWindow::drawBody() {
    const float height = ImGui::GetContentRegionAvail().y - m_ui.size("statusBar");
    beginArea("##body", height, m_ui.color("bg/base"));
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();

    const float bannerHeight = drawBanner(origin.y);
    const ImVec2 contentOrigin(origin.x, origin.y + bannerHeight);
    const ImVec2 contentSize(size.x, size.y - bannerHeight);
    if (m_status.state == ReceiverState::Running) {
        const float tabs = drawTabs(contentOrigin, contentSize.x);
        const ImVec2 areaOrigin(contentOrigin.x, contentOrigin.y + tabs);
        const ImVec2 areaSize(contentSize.x, contentSize.y - tabs);
        if (m_tab == Tab::Aircraft) {
            drawAircraftArea(areaOrigin, areaSize);
        } else {
            drawSpectrumArea(areaOrigin, areaSize);
        }
    } else {
        drawEmptyState(contentOrigin, contentSize);
    }
    ImGui::EndChild();
}

void MainWindow::drawEmptyState(ImVec2 origin, ImVec2 size) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const std::string_view title = m_ui.tr(StringId::EmptyTitle);
    const std::string_view body = m_ui.tr(StringId::EmptyBody);
    const std::string_view connect = m_ui.tr(StringId::ConnectReceiver);
    const std::string_view open = m_ui.tr(StringId::OpenRecording);

    const float measure = std::min(kMeasurePx, size.x - 2.0F * m_ui.spacing("24"));
    const ImVec2 titleSize = m_ui.textSize(TypeRole::Display, title);
    const ImVec2 bodySize = m_ui.textSize(TypeRole::Body, body, measure);
    const float buttonsWidth =
        m_ui.buttonWidth(connect) + m_ui.spacing("8") + m_ui.buttonWidth(open);
    const float blockWidth = std::max({kEmptyStateLogoPx, titleSize.x, bodySize.x, buttonsWidth});
    const float blockHeight = kEmptyStateLogoPx + m_ui.spacing("24") + titleSize.y +
                              m_ui.spacing("8") + bodySize.y + m_ui.spacing("24") +
                              m_ui.size("control");

    ImVec2 pos(origin.x + (size.x - blockWidth) * 0.5F, origin.y + (size.y - blockHeight) * 0.5F);
    drawLogoSymbol(drawList, pos, kEmptyStateLogoPx,
                   {m_ui.logoColor("rings"), m_ui.logoColor("pass"), m_ui.color("bg/base")});
    pos.y += kEmptyStateLogoPx + m_ui.spacing("24");
    m_ui.drawText(drawList, TypeRole::Display, pos, m_ui.color("text/primary"), title);
    pos.y += titleSize.y + m_ui.spacing("8");
    m_ui.drawText(drawList, TypeRole::Body, pos, m_ui.color("text/secondary"), body, measure);
    pos.y += bodySize.y + m_ui.spacing("24");
    if (m_ui.button("##connect", pos, connect, ButtonKind::Primary)) {
        m_openDevices = true;
    }
    if (m_ui.button("##open", ImVec2(pos.x + m_ui.buttonWidth(connect) + m_ui.spacing("8"), pos.y),
                    open, ButtonKind::Secondary)) {
        openRecording();
    }
}

float MainWindow::drawTabs(ImVec2 origin, float width) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float pad = m_ui.spacing("16");
    const float tabHeight = m_ui.size("control") + m_ui.spacing("8");
    drawList.AddRectFilled(ImVec2(origin.x, origin.y + tabHeight - m_ui.border()),
                           ImVec2(origin.x + width, origin.y + tabHeight),
                           m_ui.color("border/subtle"));

    // Tabs: label style, the active one in text/primary with the 2-px
    // accent "horizon rule" underneath (DESIGN.md §4–5).
    const std::string aircraftLabel = m_aircraft.aircraft().empty()
                                          ? cstr(m_ui.tr(StringId::TabAircraft))
                                          : cstr(m_ui.tr(StringId::TabAircraft)) + " · " +
                                                std::to_string(m_aircraft.aircraft().size());
    const std::array<std::pair<Tab, std::string>, 2> tabs{{
        {Tab::Spectrum, cstr(m_ui.tr(StringId::TabSpectrum))},
        {Tab::Aircraft, aircraftLabel},
    }};
    const float rule = m_ui.tokens().border("focus").value_or(2.0F);
    float x = origin.x + pad;
    for (const auto& [tab, label] : tabs) {
        const float w = m_ui.textSize(TypeRole::Label, label).x;
        ImGui::SetCursorScreenPos(ImVec2(x - m_ui.spacing("8"), origin.y));
        ImGui::PushID(static_cast<int>(tab));
        if (ImGui::InvisibleButton("##tab", ImVec2(w + 2.0F * m_ui.spacing("8"), tabHeight))) {
            m_tab = tab;
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool active = m_tab == tab;
        m_ui.drawTextCentered(drawList, TypeRole::Label, x, origin.y, tabHeight,
                              m_ui.color(active || hovered ? "text/primary" : "text/secondary"),
                              label);
        if (active) {
            drawList.AddRectFilled(ImVec2(x, origin.y + tabHeight - rule),
                                   ImVec2(x + w, origin.y + tabHeight), m_ui.color("accent"));
        }
        x += w + m_ui.spacing("24");
    }

    // FFT information, right-aligned on the spectrum tab; opens its settings.
    if (m_tab == Tab::Spectrum) {
        const double binHz = m_status.source.sampleRateSps /
                             static_cast<double>(std::max<size_t>(m_status.fftSize, 1));
        const std::string binText = binHz >= 1000.0 ? std::format("{:.2f} kHz", binHz / 1000.0)
                                                    : std::format("{:.0f} Hz", binHz);
        const std::string info = m_ui.trf(StringId::FftInfo, m_status.fftSize, binText);
        const float infoWidth = m_ui.buttonWidth(info, Icon::ChevronDown);
        const ImVec2 infoPos(origin.x + width - pad - infoWidth,
                             origin.y + (tabHeight - m_ui.size("control")) * 0.5F);
        if (m_ui.button("##fft", infoPos, info, ButtonKind::Secondary, Icon::ChevronDown)) {
            m_openFft = true;
            m_fftAnchor = ImVec2(infoPos.x + infoWidth, origin.y + tabHeight);
        }
    }
    return tabHeight + m_ui.spacing("8");
}

void MainWindow::drawSpectrumArea(ImVec2 origin, ImVec2 size) {
    if (auto tuned = m_spectrum.draw(m_ui, origin, size, vfo())) {
        tuneTo(*tuned);
    }
}

void MainWindow::drawAircraftArea(ImVec2 origin, ImVec2 size) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float pad = m_ui.spacing("16");
    const ImVec2 inner(origin.x + pad, origin.y);
    const ImVec2 innerSize(size.x - 2.0F * pad, size.y - pad);
    if (m_aircraft.aircraft().empty()) {
        // Explain what's needed, and offer to set it up.
        const std::string_view text =
            m_status.adsbActive ? m_ui.tr(StringId::AdsbListening) : m_ui.tr(StringId::AdsbHint);
        const float measure = std::min(kMeasurePx, innerSize.x);
        const ImVec2 textSize = m_ui.textSize(TypeRole::Body, text, measure);
        ImVec2 at(inner.x + (innerSize.x - measure) * 0.5F, inner.y + innerSize.y * 0.35F);
        m_ui.drawText(drawList, TypeRole::Body, at, m_ui.color("text/secondary"), text, measure);
        if (!m_status.adsbActive && m_status.isDevice) {
            at.y += textSize.y + m_ui.spacing("16");
            if (m_ui.button("##tuneAdsb", at, m_ui.tr(StringId::TuneAdsb), ButtonKind::Primary)) {
                for (const auto& preset : m_services.presets) {
                    if (preset.id == "adsb") {
                        applyPreset(preset);
                    }
                }
            }
        }
        return;
    }
    m_aircraftView.draw(m_ui, inner, innerSize, m_aircraft);
}

// --- Status bar ---------------------------------------------------------------------

void MainWindow::drawStatusBar() {
    const float height = m_ui.size("statusBar");
    beginArea("##statusbar", height, m_ui.color("bg/surface"));
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    const float pad = m_ui.spacing("16");
    const float labelGap = m_ui.spacing("4");
    const float pairGap = m_ui.spacing("16");
    const ImU32 label = m_ui.color("text/secondary");
    const ImU32 value = m_ui.color("text/primary");
    const TypeRole role = TypeRole::MonoSmall;

    drawList.AddRectFilled(origin, ImVec2(origin.x + width, origin.y + m_ui.border()),
                           m_ui.color("border/subtle"));

    // Left: product and version, then the source.
    float x = origin.x + pad;
    x += m_ui.drawTextCentered(drawList, role, x, origin.y, height, label,
                               m_ui.tr(StringId::StatusVersion)) +
         labelGap;
    x += m_ui.drawTextCentered(drawList, role, x, origin.y, height, value, m_services.versionText) +
         pairGap;
    if (m_status.state == ReceiverState::Running) {
        if (!m_status.isDevice) {
            x += m_ui.drawTextCentered(drawList, role, x, origin.y, height, label,
                                       m_ui.tr(StringId::StatusReplaying)) +
                 labelGap;
        }
        const std::string source =
            m_status.source.name + " · " + formatSampleRate(m_status.source.sampleRateSps);
        x += m_ui.drawTextCentered(drawList, role, x, origin.y, height, value, source) + pairGap;
        const uint64_t dropped = m_status.droppedBlocks + m_status.droppedRows;
        if (dropped > 0) {
            // Always an icon with the warning colour (DESIGN.md §2).
            const ImU32 warning = m_ui.color("warning");
            const float iconSize = m_ui.textSize(role, "0").y;
            m_ui.icon(drawList, Icon::Alert, ImVec2(x + iconSize * 0.5F, origin.y + height * 0.5F),
                      iconSize, warning);
            x += iconSize + labelGap;
            m_ui.drawTextCentered(drawList, role, x, origin.y, height, warning,
                                  m_ui.trf(StringId::StatusDropped, dropped));
        }
    } else {
        m_ui.drawTextCentered(drawList, role, x, origin.y, height, label,
                              m_ui.tr(StringId::StatusNoReceiver));
    }

    // Right: UTC first (everything in Zarescope runs on UTC), then local time.
    const auto now = std::chrono::system_clock::now();
    const std::string utc = formatClock(now, ClockZone::Utc);
    const std::string local = formatClock(now, ClockZone::Local);
    const std::array<std::pair<std::string_view, std::string_view>, 2> pairs{{
        {m_ui.tr(StringId::StatusUtc), utc},
        {m_ui.tr(StringId::StatusLocal), local},
    }};
    float total = pad - pairGap;
    for (const auto& [name, text] : pairs) {
        total += m_ui.textSize(role, name).x + labelGap + m_ui.textSize(role, text).x + pairGap;
    }
    x = origin.x + width - total;
    for (const auto& [name, text] : pairs) {
        x += m_ui.drawTextCentered(drawList, role, x, origin.y, height, label, name) + labelGap;
        x += m_ui.drawTextCentered(drawList, role, x, origin.y, height, value, text) + pairGap;
    }
    ImGui::EndChild();
}

// --- Popovers -----------------------------------------------------------------------

void MainWindow::openRecording() {
    if (!m_services.chooseRecording) {
        return;
    }
    if (auto path = m_services.chooseRecording()) {
        m_spectrum.clear();
        m_services.receiver->openFile(*path);
    }
}

void MainWindow::drawDevicePopup() {
    device::Receiver& receiver = *m_services.receiver;
    if (m_openDevices) {
        ImGui::OpenPopup("##devices");
        m_openDevices = false;
        m_confirmBiasTee = false;
        if (m_status.state != ReceiverState::Running) {
            receiver.scanDevices();
        }
    }
    ImGui::SetNextWindowPos(m_deviceAnchor, ImGuiCond_Always, ImVec2(1.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kPopoverWidth + 2.0F * m_ui.spacing("16"), 0.0F));
    if (!ImGui::BeginPopup("##devices")) {
        return;
    }
    auto heading = [&](std::string_view text) {
        ImGui::PushFont(m_ui.face(TypeRole::Heading).font, m_ui.face(TypeRole::Heading).sizePx);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopFont();
    };
    auto secondary = [&](const std::string& text) {
        ImGui::PushStyleColor(ImGuiCol_Text, m_ui.colorVec("text/secondary"));
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopStyleColor();
    };
    const float iconIndent = kIconPx + m_ui.spacing("8");

    const bool running = m_status.state == ReceiverState::Running;
    if (running && m_status.isDevice) {
        heading(m_status.source.name);
        std::string tuner = cstr(m_ui.tr(StringId::TunerLabel)) + ": " +
                            cstr(device::tunerName(m_status.identity.tuner));
        if (!m_status.device.serial.empty()) {
            tuner += " · " + m_ui.trf(StringId::SerialNumber, m_status.device.serial);
        }
        secondary(tuner);

        // The identity verdict, with an icon in its status colour.
        const auto verdict = m_status.identity.verdict;
        Status status = Status::Neutral;
        std::string verdictText = cstr(m_ui.tr(StringId::IdentityUnverifiable));
        if (verdict == device::IdentityVerdict::Consistent) {
            status = Status::Success;
            verdictText = cstr(m_ui.tr(StringId::IdentityConsistent));
        } else if (verdict == device::IdentityVerdict::Mismatch) {
            status = Status::Error;
            verdictText = m_ui.trf(StringId::IdentityMismatch,
                                   cstr(device::modelName(m_status.identity.claimed)));
        }
        const ImVec2 iconPos = ImGui::GetCursorScreenPos();
        m_ui.icon(*ImGui::GetWindowDrawList(),
                  status == Status::Success ? Icon::Check : Icon::Alert,
                  ImVec2(iconPos.x + kIconPx * 0.5F, iconPos.y + ImGui::GetTextLineHeight() * 0.5F),
                  kIconPx, m_ui.color(UiKit::statusToken(status)));
        ImGui::Indent(iconIndent);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kPopoverWidth - iconIndent);
        ImGui::TextUnformatted(verdictText.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Unindent(iconIndent);
        ImGui::Separator();

        // Sample rate.
        const uint32_t rate = m_status.deviceSettings.sampleRateSps;
        ImGui::TextUnformatted(cstr(m_ui.tr(StringId::SampleRateLabel)).c_str());
        ImGui::SetNextItemWidth(kPopoverWidth);
        if (ImGui::BeginCombo("##rate", formatSampleRate(rate).c_str())) {
            for (uint32_t option : device::kSampleRates) {
                if (ImGui::Selectable(formatSampleRate(option).c_str(), option == rate)) {
                    receiver.setSampleRate(option);
                }
            }
            ImGui::EndCombo();
        }
        // Frequency correction.
        ImGui::TextUnformatted(cstr(m_ui.tr(StringId::PpmLabel)).c_str());
        int ppm = m_status.deviceSettings.ppm;
        ImGui::SetNextItemWidth(kPopoverWidth);
        if (ImGui::InputInt("##ppm", &ppm, 1, 10)) {
            receiver.setPpm(std::clamp(ppm, -200, 200));
        }
        // Bias-tee: off unless the user turns it on, after a warning.
        bool biasTee = m_status.biasTee;
        if (ImGui::Checkbox(cstr(m_ui.tr(StringId::BiasTeeLabel)).c_str(), &biasTee)) {
            if (biasTee) {
                m_confirmBiasTee = true;
            } else {
                receiver.setBiasTee(false);
            }
        }
        if (m_confirmBiasTee && !m_status.biasTee) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, m_ui.colorVec("warning/soft"));
            ImGui::PushStyleColor(ImGuiCol_Border, m_ui.colorVec("warning"));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, m_ui.radius("md"));
            ImGui::BeginChild("##biasWarning", ImVec2(kPopoverWidth, 0.0F),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::TextWrapped("%s", cstr(m_ui.tr(StringId::BiasTeeWarning)).c_str());
            if (ImGui::Button(cstr(m_ui.tr(StringId::BiasTeeTurnOn)).c_str())) {
                receiver.setBiasTee(true);
                m_confirmBiasTee = false;
            }
            ImGui::SameLine();
            if (ImGui::Button(cstr(m_ui.tr(StringId::Cancel)).c_str())) {
                m_confirmBiasTee = false;
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        ImGui::Separator();
        if (ImGui::Button(cstr(m_ui.tr(StringId::CloseReceiver)).c_str())) {
            m_spectrum.clear();
            receiver.close();
            ImGui::CloseCurrentPopup();
        }
    } else if (running) {
        heading(m_status.source.name);
        secondary(m_ui.trf(StringId::RecordingFromFile,
                           formatFrequency(m_status.source.centerFrequencyHz),
                           formatSampleRate(m_status.source.sampleRateSps)));
        ImGui::Separator();
        if (ImGui::Button(cstr(m_ui.tr(StringId::CloseRecording)).c_str())) {
            m_spectrum.clear();
            receiver.close();
            ImGui::CloseCurrentPopup();
        }
    } else {
        heading(m_ui.tr(StringId::ReceiversTitle));
        const auto devices = receiver.devices();
        if (receiver.scanning()) {
            secondary(cstr(m_ui.tr(StringId::Searching)));
        } else if (devices.empty()) {
            secondary(cstr(m_ui.tr(StringId::NoReceiverFound)));
            if (ImGui::Button(cstr(m_ui.tr(StringId::SearchAgain)).c_str())) {
                receiver.scanDevices();
            }
        } else {
            for (const auto& entry : devices) {
                ImGui::PushID(static_cast<int>(entry.index));
                const std::string name = entry.product.empty() ? entry.name : entry.product;
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name.c_str());
                if (!entry.serial.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s",
                                        m_ui.trf(StringId::SerialNumber, entry.serial).c_str());
                }
                const std::string open = cstr(m_ui.tr(StringId::Open));
                ImGui::SameLine(ImGui::GetCursorPosX() + kPopoverWidth -
                                ImGui::CalcTextSize(open.c_str()).x -
                                2.0F * ImGui::GetStyle().FramePadding.x);
                if (ImGui::Button(open.c_str())) {
                    m_spectrum.clear();
                    receiver.openDevice(entry.index, device::RtlSdrSettings{
                                                         .centerFrequencyHz = kDefaultFrequencyHz,
                                                         .sampleRateSps = kDefaultSampleRate});
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::Separator();
        if (ImGui::Button(cstr(m_ui.tr(StringId::OpenRecording)).c_str())) {
            ImGui::CloseCurrentPopup();
            openRecording();
        }
    }
    ImGui::EndPopup();
}

void MainWindow::drawGainPopup() {
    if (m_openGain) {
        ImGui::OpenPopup("##gain");
        m_openGain = false;
    }
    ImGui::SetNextWindowPos(m_gainAnchor, ImGuiCond_Always);
    if (!ImGui::BeginPopup("##gain")) {
        return;
    }
    device::Receiver& receiver = *m_services.receiver;
    const auto& gains = m_status.gainsTenthsDb;
    const auto& current = m_status.deviceSettings.gainTenthsDb;
    bool automatic = !current.has_value();
    if (ImGui::Checkbox(cstr(m_ui.tr(StringId::GainAutomaticTuner)).c_str(), &automatic)) {
        // Leaving automatic starts from the middle of the tuner's range.
        receiver.setGain(automatic || gains.empty() ? std::nullopt
                                                    : std::optional<int>(gains[gains.size() / 2]));
    }
    if (!automatic && !gains.empty()) {
        // The slider moves over the tuner's own gain steps.
        int index = 0;
        for (size_t i = 0; i < gains.size(); ++i) {
            if (current && gains[i] <= *current) {
                index = static_cast<int>(i);
            }
        }
        ImGui::SetNextItemWidth(kPopoverWidth * 0.75F);
        const std::string label = formatGain(gains[static_cast<size_t>(index)]);
        if (ImGui::SliderInt("##gainSlider", &index, 0, static_cast<int>(gains.size()) - 1,
                             label.c_str())) {
            receiver.setGain(gains[static_cast<size_t>(index)]);
        }
    }
    ImGui::EndPopup();
}

void MainWindow::drawFftPopup() {
    if (m_openFft) {
        ImGui::OpenPopup("##fftSettings");
        m_openFft = false;
    }
    ImGui::SetNextWindowPos(m_fftAnchor, ImGuiCond_Always, ImVec2(1.0F, 0.0F));
    if (!ImGui::BeginPopup("##fftSettings")) {
        return;
    }
    device::Receiver& receiver = *m_services.receiver;
    const float width = kPopoverWidth * 0.75F;

    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::FftSizeLabel)).c_str());
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo("##fftSize", std::to_string(m_status.fftSize).c_str())) {
        for (size_t size : device::kFftSizes) {
            if (ImGui::Selectable(std::to_string(size).c_str(), size == m_status.fftSize)) {
                receiver.setFftSize(size);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::SmoothingLabel)).c_str());
    ImGui::SetNextItemWidth(width);
    // Shown as "how much smoothing": 0 % = raw rows, 95 % = very calm.
    float smoothing = (1.0F - m_traceSmoothing) * 100.0F;
    if (ImGui::SliderFloat("##smoothing", &smoothing, 0.0F, 95.0F, "%.0f %%")) {
        m_traceSmoothing = 1.0F - smoothing / 100.0F;
        receiver.setTraceSmoothing(m_traceSmoothing);
    }
    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::WaterfallFloorLabel)).c_str());
    ImGui::SetNextItemWidth(width);
    ImGui::SliderFloat("##floor", &m_spectrum.floorDb, -160.0F, m_spectrum.ceilingDb - 10.0F,
                       "%.0f dBFS");
    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::WaterfallCeilingLabel)).c_str());
    ImGui::SetNextItemWidth(width);
    ImGui::SliderFloat("##ceiling", &m_spectrum.ceilingDb, m_spectrum.floorDb + 10.0F, 0.0F,
                       "%.0f dBFS");
    if (ImGui::Button(cstr(m_ui.tr(StringId::FitToSignal)).c_str())) {
        m_spectrum.fitRange();
    }
    ImGui::EndPopup();
}

void MainWindow::drawAudioPopup() {
    if (m_openAudio) {
        ImGui::OpenPopup("##audio");
        m_openAudio = false;
    }
    ImGui::SetNextWindowPos(m_audioAnchor, ImGuiCond_Always);
    if (!ImGui::BeginPopup("##audio")) {
        return;
    }
    const float width = kPopoverWidth * 0.75F;
    if (!m_status.audioError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, m_ui.colorVec("error"));
        ImGui::TextWrapped("%s", m_ui.trf(StringId::NoSoundOutput, m_status.audioError).c_str());
        ImGui::PopStyleColor();
    }
    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::VolumeLabel)).c_str());
    ImGui::SetNextItemWidth(width);
    float volume = m_demod.volume * 100.0F;
    if (ImGui::SliderFloat("##volume", &volume, 0.0F, 100.0F, "%.0f %%")) {
        m_demod.volume = volume / 100.0F;
    }

    // Squelch, with the channel's live level next to it so the threshold
    // can be set just above the noise.
    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::SquelchLabel)).c_str());
    ImGui::SetNextItemWidth(width);
    constexpr float kSquelchOff = -100.0F;
    float squelch = std::max(m_demod.squelchDb, kSquelchOff);
    const std::string squelchText = squelch <= kSquelchOff
                                        ? cstr(m_ui.tr(StringId::SquelchOffValue))
                                        : formatSigned(squelch, 0) + " dBFS";
    if (ImGui::SliderFloat("##squelch", &squelch, kSquelchOff, 0.0F, squelchText.c_str())) {
        m_demod.squelchDb = squelch <= kSquelchOff ? -200.0F : squelch;
    }
    if (m_demod.mode != dsp::DemodMode::Off) {
        const Status status = m_status.squelchOpen ? Status::Live : Status::Neutral;
        ImGui::PushStyleColor(ImGuiCol_Text, m_ui.colorVec(UiKit::statusToken(status)));
        ImGui::TextUnformatted(
            m_ui.trf(StringId::SignalLevel, formatSigned(m_status.channelPowerDb, 1) + " dBFS")
                .c_str());
        ImGui::PopStyleColor();

        ImGui::TextUnformatted(cstr(m_ui.tr(StringId::BandwidthLabel)).c_str());
        ImGui::SetNextItemWidth(width);
        float kHz = static_cast<float>(m_demod.bandwidthHz / 1000.0);
        if (ImGui::SliderFloat(
                "##bandwidth", &kHz, static_cast<float>(dsp::minBandwidthHz(m_demod.mode) / 1000.0),
                static_cast<float>(dsp::maxBandwidthHz(m_demod.mode) / 1000.0), "%.1f kHz")) {
            m_demod.bandwidthHz = static_cast<double>(kHz) * 1000.0;
        }
    }
    if (m_demod.mode == dsp::DemodMode::Wfm) {
        ImGui::TextUnformatted(cstr(m_ui.tr(StringId::DeemphasisLabel)).c_str());
        if (ImGui::RadioButton(cstr(m_ui.tr(StringId::Deemphasis50)).c_str(),
                               m_demod.deemphasisUs == 50.0)) {
            m_demod.deemphasisUs = 50.0;
        }
        if (ImGui::RadioButton(cstr(m_ui.tr(StringId::Deemphasis75)).c_str(),
                               m_demod.deemphasisUs == 75.0)) {
            m_demod.deemphasisUs = 75.0;
        }
    }
    ImGui::EndPopup();
}

void MainWindow::drawPresetsPopup() {
    if (m_openPresets) {
        ImGui::OpenPopup("##presetList");
        m_openPresets = false;
    }
    ImGui::SetNextWindowPos(m_presetsAnchor, ImGuiCond_Always);
    if (!ImGui::BeginPopup("##presetList")) {
        return;
    }
    for (const device::Preset& preset : m_services.presets) {
        const auto name = presetName(preset.id);
        const std::string label = name ? cstr(m_ui.tr(*name)) : preset.id;
        ImGui::PushID(preset.id.c_str());
        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None,
                              ImVec2(kPopoverWidth * 0.6F, 0.0F))) {
            applyPreset(preset);
        }
        ImGui::SameLine();
        ImGui::PushFont(m_ui.face(TypeRole::MonoData).font, m_ui.face(TypeRole::MonoData).sizePx);
        ImGui::TextDisabled("%s", formatFrequency(preset.frequencyHz).c_str());
        ImGui::PopFont();
        ImGui::PopID();
    }
    ImGui::EndPopup();
}

void MainWindow::drawSettingsPopup(ShellState& state) {
    if (m_openSettings) {
        ImGui::OpenPopup("##settings");
        m_openSettings = false;
    }
    ImGui::SetNextWindowPos(m_settingsAnchor, ImGuiCond_Always, ImVec2(1.0F, 0.0F));
    if (!ImGui::BeginPopup("##settings")) {
        return;
    }
    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::ThemeLabel)).c_str());
    if (ImGui::RadioButton(cstr(m_ui.tr(StringId::ThemeDark)).c_str(),
                           state.variant == theme::ThemeVariant::Dark)) {
        state.variant = theme::ThemeVariant::Dark;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(cstr(m_ui.tr(StringId::ThemeLight)).c_str(),
                           state.variant == theme::ThemeVariant::Light)) {
        state.variant = theme::ThemeVariant::Light;
    }
    ImGui::Separator();
    // Language names are written in their own language, so each is
    // recognisable whatever the current one is.
    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::StationLabel)).c_str());
    ImGui::SetNextItemWidth(kPopoverWidth * 0.5F);
    ImGui::InputDouble(cstr(m_ui.tr(StringId::LatitudeLabel)).c_str(), &m_stationLat, 0.0, 0.0,
                       "%.4f");
    ImGui::SetNextItemWidth(kPopoverWidth * 0.5F);
    ImGui::InputDouble(cstr(m_ui.tr(StringId::LongitudeLabel)).c_str(), &m_stationLon, 0.0, 0.0,
                       "%.4f");
    m_stationLat = std::clamp(m_stationLat, -90.0, 90.0);
    m_stationLon = std::clamp(m_stationLon, -180.0, 180.0);
    if (ImGui::Button(cstr(m_ui.tr(StringId::UseLocation)).c_str())) {
        m_aircraft.setStation(adsb::LatLon{m_stationLat, m_stationLon});
    }
    ImGui::Separator();
    ImGui::TextUnformatted(cstr(m_ui.tr(StringId::LanguageLabel)).c_str());
    if (ImGui::RadioButton("English", state.language == Language::English)) {
        state.language = Language::English;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Română", state.language == Language::Romanian)) {
        state.language = Language::Romanian;
    }
    ImGui::EndPopup();
}

}  // namespace zarescope::ui
