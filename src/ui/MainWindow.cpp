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
    ImGui::PopStyleVar(2);

    ImGui::End();
}

// --- Top bar ----------------------------------------------------------------------

void MainWindow::drawTopBar(ShellState& state) {
    const float height = m_ui.size("topBar");
    beginArea("##topbar", height, m_ui.color("bg/surface"));

    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    const float pad = m_ui.spacing("16");
    const float gap = m_ui.spacing("8");
    const float controlY = origin.y + (height - m_ui.size("control")) * 0.5F;
    const float chipY = origin.y + (height - m_ui.size("controlCompact")) * 0.5F;
    const bool running = m_status.state == ReceiverState::Running;
    const bool isDevice = running && m_status.isDevice;

    // Left: logo, frequency, gain.
    drawLogoSymbol(drawList, ImVec2(origin.x + pad, origin.y + (height - kTopBarLogoPx) * 0.5F),
                   kTopBarLogoPx,
                   {m_ui.logoColor("rings"), m_ui.logoColor("pass"), m_ui.color("bg/surface")});
    float x = origin.x + pad + kTopBarLogoPx + m_ui.spacing("24");
    if (running) {
        const float freqY = origin.y + (height - m_frequency.height(m_ui)) * 0.5F;
        const int64_t shown = m_status.source.centerFrequencyHz;
        if (auto tuned = m_frequency.draw(m_ui, ImVec2(x, freqY), shown, isDevice,
                                          device::kMinFrequencyHz, device::kMaxFrequencyHz)) {
            m_services.receiver->setCenterFrequency(*tuned);
        }
        x += m_frequency.width(m_ui, shown) + m_ui.spacing("24");
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
    }

    // Right, from the edge inwards: theme switch, record, live chip, device chip.
    const std::array<std::string_view, 2> themes{m_ui.tr(StringId::ThemeDark),
                                                 m_ui.tr(StringId::ThemeLight)};
    float right = origin.x + width - pad;
    right -= m_ui.segmentedWidth(themes.data(), 2);
    int selected = state.variant == theme::ThemeVariant::Dark ? 0 : 1;
    if (m_ui.segmented("theme", ImVec2(right, controlY), themes.data(), 2, selected,
                       m_ui.tr(StringId::ThemeLabel))) {
        state.variant = selected == 0 ? theme::ThemeVariant::Dark : theme::ThemeVariant::Light;
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
        drawSpectrumArea(contentOrigin, contentSize);
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

void MainWindow::drawSpectrumArea(ImVec2 origin, ImVec2 size) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const float pad = m_ui.spacing("16");
    const float tabHeight = m_ui.size("control") + m_ui.spacing("8");

    // Tab strip: one tab for now, marked by the 2-px accent "horizon rule".
    const std::string_view tab = m_ui.tr(StringId::TabSpectrum);
    const float tabWidth =
        m_ui.drawTextCentered(drawList, TypeRole::Label, origin.x + pad, origin.y, tabHeight,
                              m_ui.color("text/primary"), tab);
    const float rule = m_ui.tokens().border("focus").value_or(2.0F);
    drawList.AddRectFilled(ImVec2(origin.x, origin.y + tabHeight - m_ui.border()),
                           ImVec2(origin.x + size.x, origin.y + tabHeight),
                           m_ui.color("border/subtle"));
    drawList.AddRectFilled(ImVec2(origin.x + pad, origin.y + tabHeight - rule),
                           ImVec2(origin.x + pad + tabWidth, origin.y + tabHeight),
                           m_ui.color("accent"));

    // FFT information, right-aligned; opens the FFT settings.
    const double binHz =
        m_status.source.sampleRateSps / static_cast<double>(std::max<size_t>(m_status.fftSize, 1));
    const std::string binText = binHz >= 1000.0 ? std::format("{:.2f} kHz", binHz / 1000.0)
                                                : std::format("{:.0f} Hz", binHz);
    const std::string info = m_ui.trf(StringId::FftInfo, m_status.fftSize, binText);
    const float infoWidth = m_ui.buttonWidth(info, Icon::ChevronDown);
    const ImVec2 infoPos(origin.x + size.x - pad - infoWidth,
                         origin.y + (tabHeight - m_ui.size("control")) * 0.5F);
    if (m_ui.button("##fft", infoPos, info, ButtonKind::Secondary, Icon::ChevronDown)) {
        m_openFft = true;
        m_fftAnchor = ImVec2(infoPos.x + infoWidth, origin.y + tabHeight);
    }

    const ImVec2 viewPos(origin.x, origin.y + tabHeight + m_ui.spacing("8"));
    const ImVec2 viewSize(size.x, size.y - tabHeight - m_ui.spacing("8"));
    const bool canTune = m_status.isDevice;
    if (auto tuned =
            m_spectrum.draw(m_ui, viewPos, viewSize, m_status.source.centerFrequencyHz, canTune)) {
        m_services.receiver->setCenterFrequency(
            std::clamp(*tuned, device::kMinFrequencyHz, device::kMaxFrequencyHz));
    }
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

}  // namespace zarescope::ui
