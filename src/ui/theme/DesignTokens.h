// SPDX-License-Identifier: GPL-3.0-or-later
//
// DesignTokens: the Zarescope design system as data.
//
// design/tokens/zarescope.tokens.json is the single source of truth for every
// colour, size and Dear ImGui style value (design/docs/DESIGN.md explains how
// to use them). This class loads that file and checks it, so a broken token
// file stops the app at start-up with a clear message instead of showing
// wrong colours.
//
// It is plain C++ with no ImGui dependency, so the tests can check it without
// a window. src/ui/theme/ImGuiTheme.h turns it into an ImGuiStyle.

#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace zarescope::theme {

// An opaque or translucent colour, each channel in 0..1.
struct Rgba {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;

    bool operator==(const Rgba&) const = default;
};

enum class ThemeVariant { Dark, Light };

// The token file's name for a variant ("dark" / "light").
std::string_view variantName(ThemeVariant variant);

// Parses "#RRGGBB" or "#RRGGBBAA" (case-insensitive). Anything else gives
// std::nullopt.
std::optional<Rgba> parseHexColor(std::string_view text);

// One step of the type scale, e.g. "body" = Inter 400, 13/18 px.
struct TypeStyle {
    enum class Family { Ui, Mono };

    Family family = Family::Ui;
    int weight = 400;
    float sizePx = 13.0F;
    float lineHeightPx = 18.0F;
    float tracking = 0.0F;  // fraction of the font size (-0.01 = -1 %)
    bool uppercase = false;
};

// Thrown when the token file is missing, isn't valid JSON, or breaks one of
// the rules checked in DesignTokens::fromJson().
class TokenError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DesignTokens {
public:
    // A Dear ImGui style value from the token file: one number (rounding,
    // border size) or two (padding, spacing).
    using StyleValue = std::vector<float>;

    // Parses and validates the token file's contents. Throws TokenError.
    static DesignTokens fromJson(std::string_view jsonText);
    // Reads the file, then fromJson(). Throws TokenError.
    static DesignTokens fromFile(const std::filesystem::path& path);

    [[nodiscard]] const std::string& version() const { return m_version; }
    [[nodiscard]] ThemeVariant defaultVariant() const { return m_defaultVariant; }

    // A colour token such as "accent" or "bg/surface" in the given theme.
    [[nodiscard]] std::optional<Rgba> color(ThemeVariant variant, std::string_view token) const;

    // A colour expression as the token file's imgui section writes it:
    //   "accent"         a colour token
    //   "transparent"    fully transparent
    //   "#000000 @50%"   a literal colour with an opacity
    [[nodiscard]] std::optional<Rgba> resolveColor(ThemeVariant variant,
                                                   std::string_view expression) const;

    // The logo's two colours in a theme: "rings" and "pass". The light
    // theme's pass amber (#C06A00) is for graphics only, so it is not the
    // accent token.
    [[nodiscard]] std::optional<Rgba> logoColor(ThemeVariant variant, std::string_view part) const;

    // Every colour token of a theme, by name.
    [[nodiscard]] const std::map<std::string, Rgba, std::less<>>& palette(
        ThemeVariant variant) const;

    // A step of the type scale ("body", "mono/freq"…), or nullptr.
    [[nodiscard]] const TypeStyle* typeStyle(std::string_view role) const;

    // Named sizes from the "size" section ("control", "topBar", "statusBar"…).
    [[nodiscard]] std::optional<float> size(std::string_view name) const;
    // The 4-px spacing scale ("2", "4", "8"…).
    [[nodiscard]] std::optional<float> spacing(std::string_view step) const;
    // Corner radii ("xs", "sm", "md", "lg", "xl", "pill").
    [[nodiscard]] std::optional<float> radius(std::string_view name) const;
    // Border widths ("default", "focus", "focusOffset").
    [[nodiscard]] std::optional<float> border(std::string_view name) const;

    // ImGuiCol name (without the ImGuiCol_ prefix) → colour expression.
    [[nodiscard]] const std::map<std::string, std::string, std::less<>>& imguiColors() const {
        return m_imguiColors;
    }
    // ImGuiStyle field name → value.
    [[nodiscard]] const std::map<std::string, StyleValue, std::less<>>& imguiStyle() const {
        return m_imguiStyle;
    }

    // The "Zare Deep" waterfall ramp: index 0 = noise floor, 255 = ceiling.
    // The same in both themes.
    [[nodiscard]] const std::array<Rgba, 256>& waterfallLut() const { return m_waterfallLut; }

private:
    DesignTokens() = default;

    using NumberMap = std::map<std::string, float, std::less<>>;

    std::string m_version;
    ThemeVariant m_defaultVariant = ThemeVariant::Dark;
    std::map<std::string, Rgba, std::less<>> m_dark;
    std::map<std::string, Rgba, std::less<>> m_light;
    std::map<std::string, Rgba, std::less<>> m_logoDark;
    std::map<std::string, Rgba, std::less<>> m_logoLight;
    std::map<std::string, TypeStyle, std::less<>> m_typeScale;
    NumberMap m_sizes;
    NumberMap m_spacing;
    NumberMap m_radius;
    NumberMap m_border;
    std::map<std::string, std::string, std::less<>> m_imguiColors;
    std::map<std::string, StyleValue, std::less<>> m_imguiStyle;
    std::array<Rgba, 256> m_waterfallLut{};
};

}  // namespace zarescope::theme
