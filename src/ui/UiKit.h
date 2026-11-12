// SPDX-License-Identifier: GPL-3.0-or-later
//
// UiKit: the design system at the point of use.
//
// Every screen component draws through a UiKit: colours, sizes and type
// come from the tokens by name, text from the string table, and the shared
// widgets (buttons, chips, segmented controls, banners, the few line icons)
// look the same everywhere. A widget never names a hex value, a pixel size
// or a font file.
//
// The widgets draw with ImDrawList at positions the caller computes (the
// shell is a fixed layout) and use InvisibleButton for input, so they still
// take part in keyboard navigation and get the focus ring.

#pragma once

#include "ui/Strings.h"
#include "ui/Typography.h"
#include "ui/theme/DesignTokens.h"

#include <imgui.h>

#include <string>
#include <string_view>

namespace zarescope::ui {

enum class ButtonKind { Primary, Secondary, Danger };

// The status colours of DESIGN.md §2. Always paired with a label or icon.
enum class Status { Neutral, Live, Recording, PassImminent, Success, Warning, Error };

// Line icons drawn with primitives (the 24-grid set in design/icons arrives
// with an SVG rasterizer later).
enum class Icon { None, Dot, Record, Stop, Alert, Check, ChevronDown, Settings, Speaker };

class UiKit {
public:
    UiKit(const theme::DesignTokens& tokens, const Typography& typography);

    // Call at the start of every frame.
    void beginFrame(theme::ThemeVariant variant, Language language);

    [[nodiscard]] theme::ThemeVariant variant() const { return m_variant; }
    [[nodiscard]] Language language() const { return m_language; }
    [[nodiscard]] const theme::DesignTokens& tokens() const { return m_tokens; }

    // --- Tokens ---------------------------------------------------------------
    [[nodiscard]] ImU32 color(std::string_view token) const;
    [[nodiscard]] ImVec4 colorVec(std::string_view token) const;
    [[nodiscard]] ImU32 logoColor(std::string_view part) const;
    [[nodiscard]] float size(std::string_view name) const;
    [[nodiscard]] float spacing(std::string_view step) const;
    [[nodiscard]] float radius(std::string_view name) const;
    [[nodiscard]] float border() const;
    // The foreground and soft background token names of a status.
    [[nodiscard]] static std::string_view statusToken(Status status);
    [[nodiscard]] static std::string statusSoftToken(Status status);

    // --- Text -----------------------------------------------------------------
    [[nodiscard]] std::string_view tr(StringId id) const { return text(id, m_language); }
    template <typename... Args>
    [[nodiscard]] std::string trf(StringId id, Args&&... args) const {
        return textf(id, m_language, std::forward<Args>(args)...);
    }
    [[nodiscard]] const Typography::Face& face(TypeRole role) const {
        return m_typography.face(role);
    }
    [[nodiscard]] ImVec2 textSize(TypeRole role, std::string_view text,
                                  float wrapWidth = 0.0F) const;
    void drawText(ImDrawList& drawList, TypeRole role, ImVec2 pos, ImU32 color,
                  std::string_view text, float wrapWidth = 0.0F) const;
    // Draws text vertically centred in [top, top + height]; returns its width.
    float drawTextCentered(ImDrawList& drawList, TypeRole role, float x, float top, float height,
                           ImU32 color, std::string_view text) const;

    // --- Widgets --------------------------------------------------------------
    [[nodiscard]] float buttonWidth(std::string_view label, Icon icon = Icon::None) const;
    // A button of the control height at `pos`. Returns true when clicked.
    bool button(const char* id, ImVec2 pos, std::string_view label, ButtonKind kind,
                Icon icon = Icon::None, bool enabled = true);
    // A square, icon-only button; `tooltip` names it (it has no label).
    bool iconButton(const char* id, ImVec2 pos, Icon icon, std::string_view tooltip,
                    bool active = false);
    [[nodiscard]] float iconButtonWidth() const { return size("control"); }

    [[nodiscard]] float chipWidth(std::string_view label, bool withDot) const;
    // A pill chip of the compact control height. Returns true when clicked
    // (only when `clickable`). Status chips show a dot in the status colour.
    bool chip(const char* id, ImVec2 pos, std::string_view label, Status status, bool clickable,
              bool withDot);

    [[nodiscard]] float segmentedWidth(const std::string_view* labels, int count) const;
    bool segmented(const char* id, ImVec2 pos, const std::string_view* labels, int count,
                   int& selected, std::string_view tooltip = {});

    // A switch (DESIGN.md: 36×20 pill, 14-px knob) with its label to the
    // right, in a row of the control height. The whole row is clickable.
    // Returns true when the user flipped it (`on` is then updated).
    bool toggle(const char* id, ImVec2 pos, bool& on, std::string_view label, bool enabled = true);
    [[nodiscard]] float toggleWidth(std::string_view label) const;

    // A status banner across [min.x, max.x] at min.y, the banner token's
    // height: soft background, 1-px border in the status colour, an icon and
    // the message, then (optional) a progress bar for `progress` in 0…1 and
    // up to two buttons. Returns true when `action` is clicked; a click on
    // `secondaryAction` sets *secondaryClicked instead.
    bool banner(const char* id, ImVec2 min, float width, Status status, Icon icon,
                std::string_view badge, std::string_view message, std::string_view action,
                float progress = -1.0F, std::string_view secondaryAction = {},
                bool* secondaryClicked = nullptr);

    void icon(ImDrawList& drawList, Icon icon, ImVec2 center, float size, ImU32 color) const;

private:
    const theme::DesignTokens& m_tokens;
    const Typography& m_typography;
    theme::ThemeVariant m_variant = theme::ThemeVariant::Dark;
    Language m_language = Language::English;
};

}  // namespace zarescope::ui
