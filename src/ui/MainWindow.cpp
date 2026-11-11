// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main window's shell. See MainWindow.h for the layout.
//
// The shell is a fixed layout, so it draws with ImDrawList at computed
// positions instead of ImGui's flowing layout. The only interactive parts
// (the theme switch) use InvisibleButton, so they still take part in
// keyboard navigation and get the focus ring.
//
// Every colour and size comes from the design tokens. The few lengths that
// aren't tokens are named constants below, each citing the spec it comes from.

#include "ui/MainWindow.h"

#include "ui/Clock.h"
#include "ui/LogoMark.h"
#include "ui/theme/ImGuiTheme.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <utility>

namespace zarescope::ui {

namespace {

// DESIGN.md §6: the top bar shows the small symbol at 28 px.
constexpr float kTopBarLogoPx = 28.0F;
// The empty state's symbol, at the full drawing's native 64-px grid.
constexpr float kEmptyStateLogoPx = 64.0F;
// Body text measure: about 65 characters of 13-px Inter.
constexpr float kEmptyStateMeasurePx = 440.0F;

ImVec2 add(ImVec2 a, ImVec2 b) { return {a.x + b.x, a.y + b.y}; }

ImVec2 textSize(const Typography::Face& face, std::string_view text, float wrapWidth = 0.0F) {
    return face.font->CalcTextSizeA(face.sizePx, FLT_MAX, wrapWidth, text.data(),
                                    text.data() + text.size());
}

void drawText(ImDrawList& drawList, const Typography::Face& face, ImVec2 pos, ImU32 color,
              std::string_view text, float wrapWidth = 0.0F) {
    drawList.AddText(face.font, face.sizePx, pos, color, text.data(), text.data() + text.size(),
                     wrapWidth);
}

// Pushes the style for a full-width, square, borderless bar or area.
class AreaStyle {
public:
    explicit AreaStyle(ImU32 background) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
    }
    ~AreaStyle() {
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
    }
    AreaStyle(const AreaStyle&) = delete;
    AreaStyle& operator=(const AreaStyle&) = delete;
    AreaStyle(AreaStyle&&) = delete;
    AreaStyle& operator=(AreaStyle&&) = delete;
};

constexpr ImGuiWindowFlags kAreaFlags = ImGuiWindowFlags_NoScrollbar |
                                        ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoSavedSettings;

}  // namespace

MainWindow::MainWindow(const theme::DesignTokens& tokens, const Typography& typography,
                       std::string versionText)
    : m_tokens(tokens), m_typography(typography), m_versionText(std::move(versionText)) {}

ImU32 MainWindow::color(std::string_view token) const {
    auto rgba = m_tokens.color(m_state->variant, token);
    IM_ASSERT(rgba && "unknown colour token");
    return rgba ? toImU32(*rgba) : ImGui::GetColorU32(ImGuiCol_Text);
}

float MainWindow::sizeToken(std::string_view name) const {
    auto value = m_tokens.size(name);
    IM_ASSERT(value && "unknown size token");
    return value.value_or(0.0F);
}

float MainWindow::spacingToken(std::string_view step) const {
    auto value = m_tokens.spacing(step);
    IM_ASSERT(value && "unknown spacing token");
    return value.value_or(0.0F);
}

float MainWindow::radiusToken(std::string_view name) const {
    auto value = m_tokens.radius(name);
    IM_ASSERT(value && "unknown radius token");
    return value.value_or(0.0F);
}

void MainWindow::draw(ShellState& state) {
    m_state = &state;

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

    drawTopBar();
    drawBody();
    drawStatusBar();

    ImGui::End();
    ImGui::PopStyleVar(3);
    m_state = nullptr;
}

void MainWindow::drawTopBar() {
    const float height = sizeToken("topBar");
    const AreaStyle style(color("bg/surface"));
    ImGui::BeginChild("##topbar", ImVec2(0.0F, height), ImGuiChildFlags_None, kAreaFlags);

    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    const float padding = spacingToken("16");

    // Logo, vertically centred.
    const LogoColors logoColors{
        toImU32(*m_tokens.logoColor(m_state->variant, "rings")),
        toImU32(*m_tokens.logoColor(m_state->variant, "pass")),
        color("bg/surface"),
    };
    drawLogoSymbol(drawList, ImVec2(origin.x + padding, origin.y + (height - kTopBarLogoPx) * 0.5F),
                   kTopBarLogoPx, logoColors);

    // Right-hand cluster: receiver chip, then the theme switch.
    const std::array<std::string_view, 2> themeLabels{tr(StringId::ThemeDark),
                                                      tr(StringId::ThemeLight)};
    const float switchWidth = segmentedWidth(themeLabels.data(), 2);
    const float switchX = origin.x + width - padding - switchWidth;
    int selected = m_state->variant == theme::ThemeVariant::Dark ? 0 : 1;
    if (segmented("theme", ImVec2(switchX, origin.y + (height - sizeToken("control")) * 0.5F),
                  themeLabels.data(), 2, selected)) {
        m_state->variant = selected == 0 ? theme::ThemeVariant::Dark : theme::ThemeVariant::Light;
    }

    const std::string_view chipLabel = tr(StringId::ReceiverChipNone);
    const float chipX = switchX - spacingToken("8") - chipWidth(chipLabel);
    chip(ImVec2(chipX, origin.y + (height - sizeToken("controlCompact")) * 0.5F), chipLabel);

    // 1-px rule along the bottom edge.
    const float rule = m_tokens.border("default").value_or(1.0F);
    drawList.AddRectFilled(ImVec2(origin.x, origin.y + height - rule),
                           ImVec2(origin.x + width, origin.y + height), color("border/subtle"));

    ImGui::EndChild();
}

void MainWindow::drawBody() {
    const float height = ImGui::GetContentRegionAvail().y - sizeToken("statusBar");
    const AreaStyle style(color("bg/base"));
    ImGui::BeginChild("##body", ImVec2(0.0F, height), ImGuiChildFlags_None, kAreaFlags);

    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();

    // Empty state: symbol, display title, one paragraph. A left-aligned
    // block, centred in the body.
    const auto& titleFace = m_typography.face(TypeRole::Display);
    const auto& bodyFace = m_typography.face(TypeRole::Body);
    const std::string_view title = tr(StringId::EmptyTitle);
    const std::string_view body = tr(StringId::EmptyBody);

    const float measure = std::min(kEmptyStateMeasurePx, size.x - 2.0F * spacingToken("24"));
    const ImVec2 titleSize = textSize(titleFace, title);
    const ImVec2 bodySize = textSize(bodyFace, body, measure);
    const float blockWidth = std::max({kEmptyStateLogoPx, titleSize.x, bodySize.x});
    const float gapLogo = spacingToken("24");
    const float gapTitle = spacingToken("8");
    const float blockHeight = kEmptyStateLogoPx + gapLogo + titleSize.y + gapTitle + bodySize.y;

    ImVec2 pos(origin.x + (size.x - blockWidth) * 0.5F, origin.y + (size.y - blockHeight) * 0.5F);
    const LogoColors logoColors{
        toImU32(*m_tokens.logoColor(m_state->variant, "rings")),
        toImU32(*m_tokens.logoColor(m_state->variant, "pass")),
        color("bg/base"),
    };
    drawLogoSymbol(drawList, pos, kEmptyStateLogoPx, logoColors);
    pos.y += kEmptyStateLogoPx + gapLogo;
    drawText(drawList, titleFace, pos, color("text/primary"), title);
    pos.y += titleSize.y + gapTitle;
    drawText(drawList, bodyFace, pos, color("text/secondary"), body, measure);

    ImGui::EndChild();
}

void MainWindow::drawStatusBar() {
    const float height = sizeToken("statusBar");
    const AreaStyle style(color("bg/surface"));
    ImGui::BeginChild("##statusbar", ImVec2(0.0F, height), ImGuiChildFlags_None, kAreaFlags);

    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    const float padding = spacingToken("16");
    const float labelGap = spacingToken("4");
    const float pairGap = spacingToken("16");
    const auto& face = m_typography.face(TypeRole::MonoSmall);
    const ImU32 labelColor = color("text/secondary");
    const ImU32 valueColor = color("text/primary");

    const float rule = m_tokens.border("default").value_or(1.0F);
    drawList.AddRectFilled(origin, ImVec2(origin.x + width, origin.y + rule),
                           color("border/subtle"));

    const float textY = origin.y + (height - textSize(face, "0").y) * 0.5F;

    // Left: product name and version.
    float x = origin.x + padding;
    const std::string_view name = tr(StringId::StatusVersion);
    drawText(drawList, face, ImVec2(x, textY), labelColor, name);
    x += textSize(face, name).x + labelGap;
    drawText(drawList, face, ImVec2(x, textY), valueColor, m_versionText);

    // Right: UTC first (everything in Zarescope runs on UTC), then local time.
    const auto now = std::chrono::system_clock::now();
    const std::string utc = formatClock(now, ClockZone::Utc);
    const std::string local = formatClock(now, ClockZone::Local);
    const std::array<std::pair<std::string_view, std::string_view>, 2> pairs{{
        {tr(StringId::StatusUtc), utc},
        {tr(StringId::StatusLocal), local},
    }};
    float total = 0.0F;
    for (const auto& [label, value] : pairs) {
        total += textSize(face, label).x + labelGap + textSize(face, value).x;
    }
    total += pairGap * static_cast<float>(pairs.size() - 1);
    x = origin.x + width - padding - total;
    for (const auto& [label, value] : pairs) {
        drawText(drawList, face, ImVec2(x, textY), labelColor, label);
        x += textSize(face, label).x + labelGap;
        drawText(drawList, face, ImVec2(x, textY), valueColor, value);
        x += textSize(face, value).x + pairGap;
    }

    ImGui::EndChild();
}

float MainWindow::chipWidth(std::string_view label) const {
    return textSize(m_typography.face(TypeRole::Label), label).x + 2.0F * spacingToken("12");
}

float MainWindow::chip(ImVec2 pos, std::string_view label) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const auto& face = m_typography.face(TypeRole::Label);
    const float height = sizeToken("controlCompact");
    const float width = chipWidth(label);
    const ImVec2 max(pos.x + width, pos.y + height);
    const float rounding = radiusToken("pill");
    const float rule = m_tokens.border("default").value_or(1.0F);

    drawList.AddRectFilled(pos, max, color("bg/raised"), rounding);
    drawList.AddRect(pos, max, color("border/subtle"), rounding, rule);
    const ImVec2 labelSize = textSize(face, label);
    drawText(drawList, face,
             ImVec2(pos.x + (width - labelSize.x) * 0.5F, pos.y + (height - labelSize.y) * 0.5F),
             color("text/secondary"), label);
    return width;
}

float MainWindow::segmentedWidth(const std::string_view* labels, int count) const {
    const auto& face = m_typography.face(TypeRole::Label);
    float width = 2.0F * spacingToken("2");
    for (int i = 0; i < count; ++i) {
        width += textSize(face, labels[i]).x + 2.0F * spacingToken("12");
    }
    return width;
}

bool MainWindow::segmented(const char* id, ImVec2 pos, const std::string_view* labels, int count,
                           int& selected) {
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const auto& face = m_typography.face(TypeRole::Label);
    const float height = sizeToken("control");
    const float inset = spacingToken("2");
    const float padX = spacingToken("12");
    const float rule = m_tokens.border("default").value_or(1.0F);
    const ImVec2 max(pos.x + segmentedWidth(labels, count), pos.y + height);

    // Track.
    drawList.AddRectFilled(pos, max, color("bg/raised"), radiusToken("md"));
    drawList.AddRect(pos, max, color("border/subtle"), radiusToken("md"), rule);

    bool changed = false;
    ImGui::PushID(id);
    float x = pos.x + inset;
    for (int i = 0; i < count; ++i) {
        const ImVec2 labelSize = textSize(face, labels[i]);
        const ImVec2 segMin(x, pos.y + inset);
        const ImVec2 segSize(labelSize.x + 2.0F * padX, height - 2.0F * inset);

        ImGui::SetCursorScreenPos(segMin);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##segment", segSize) && selected != i) {
            selected = i;
            changed = true;
        }
        ImGui::SetItemTooltip("%s", std::string(tr(StringId::ThemeLabel)).c_str());
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        const bool isSelected = selected == i;
        if (isSelected || hovered) {
            drawList.AddRectFilled(segMin, add(segMin, segSize),
                                   color(isSelected ? "accent/soft" : "bg/hover"),
                                   radiusToken("sm"));
        }
        drawText(drawList, face, ImVec2(x + padX, segMin.y + (segSize.y - labelSize.y) * 0.5F),
                 color(isSelected || hovered ? "text/primary" : "text/secondary"), labels[i]);
        x += segSize.x;
    }
    ImGui::PopID();
    return changed;
}

}  // namespace zarescope::ui
