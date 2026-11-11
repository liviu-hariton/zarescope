// SPDX-License-Identifier: GPL-3.0-or-later
//
// Loading and validating the design token file. See DesignTokens.h.

#include "ui/theme/DesignTokens.h"

#include <nlohmann/json.hpp>

#include <charconv>
#include <fstream>
#include <sstream>

namespace zarescope::theme {

namespace {

using nlohmann::json;

// The value of one hex digit, or -1.
int hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
}

// The object at `key`, or a TokenError naming the missing section.
const json& section(const json& parent, const char* key, std::string_view where) {
    auto it = parent.find(key);
    if (it == parent.end() || !it->is_object()) {
        throw TokenError("design tokens: missing object \"" + std::string(where) + "\"");
    }
    return *it;
}

// Reads every plain number of an object into a map; other values (arrays,
// strings) are skipped. "size" has one array ("icon"), which the UI doesn't
// look up by name.
std::map<std::string, float, std::less<>> numbers(const json& object) {
    std::map<std::string, float, std::less<>> out;
    for (const auto& [key, value] : object.items()) {
        if (value.is_number()) {
            out.emplace(key, value.get<float>());
        }
    }
    return out;
}

std::map<std::string, Rgba, std::less<>> parsePalette(const json& object,
                                                      std::string_view themeName) {
    std::map<std::string, Rgba, std::less<>> out;
    for (const auto& [key, value] : object.items()) {
        if (!value.is_string()) {
            throw TokenError("design tokens: colour \"" + key + "\" in theme \"" +
                             std::string(themeName) + "\" is not a string");
        }
        auto rgba = parseHexColor(value.get<std::string>());
        if (!rgba) {
            throw TokenError("design tokens: colour \"" + key + "\" in theme \"" +
                             std::string(themeName) +
                             "\" is not #RRGGBB: " + value.get<std::string>());
        }
        out.emplace(key, *rgba);
    }
    return out;
}

TypeStyle parseTypeStyle(const std::string& role, const json& value) {
    TypeStyle style;
    const std::string family = value.value("font", "");
    if (family == "ui") {
        style.family = TypeStyle::Family::Ui;
    } else if (family == "mono") {
        style.family = TypeStyle::Family::Mono;
    } else {
        throw TokenError("design tokens: type style \"" + role + "\" has unknown font \"" + family +
                         "\"");
    }
    if (!value.contains("size") || !value.contains("lineHeight") || !value.contains("weight")) {
        throw TokenError("design tokens: type style \"" + role +
                         "\" needs weight, size and lineHeight");
    }
    style.weight = value.at("weight").get<int>();
    style.sizePx = value.at("size").get<float>();
    style.lineHeightPx = value.at("lineHeight").get<float>();
    style.tracking = value.value("tracking", 0.0F);
    style.uppercase = value.value("uppercase", false);
    return style;
}

template <typename Map>
std::optional<float> lookup(const Map& map, std::string_view name) {
    auto it = map.find(name);
    if (it == map.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace

std::string_view variantName(ThemeVariant variant) {
    return variant == ThemeVariant::Dark ? "dark" : "light";
}

std::optional<Rgba> parseHexColor(std::string_view text) {
    if ((text.size() != 7 && text.size() != 9) || text.front() != '#') {
        return std::nullopt;
    }
    std::array<float, 4> channels{0.0F, 0.0F, 0.0F, 1.0F};
    const size_t count = (text.size() - 1) / 2;
    for (size_t i = 0; i < count; ++i) {
        const int high = hexDigit(text[1 + i * 2]);
        const int low = hexDigit(text[2 + i * 2]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        channels[i] = static_cast<float>(high * 16 + low) / 255.0F;
    }
    return Rgba{channels[0], channels[1], channels[2], channels[3]};
}

DesignTokens DesignTokens::fromFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw TokenError("design tokens: cannot open " + path.string());
    }
    std::ostringstream text;
    text << in.rdbuf();
    return fromJson(text.str());
}

DesignTokens DesignTokens::fromJson(std::string_view jsonText) {
    json root;
    try {
        root = json::parse(jsonText);
    } catch (const json::parse_error& e) {
        throw TokenError(std::string("design tokens: invalid JSON: ") + e.what());
    }
    if (!root.is_object()) {
        throw TokenError("design tokens: the file must hold one JSON object");
    }

    DesignTokens tokens;
    try {
        tokens.m_version = root.value("version", "");

        const std::string defaultTheme = root.value("defaultTheme", "dark");
        if (defaultTheme != "dark" && defaultTheme != "light") {
            throw TokenError("design tokens: defaultTheme must be \"dark\" or \"light\"");
        }
        tokens.m_defaultVariant = defaultTheme == "dark" ? ThemeVariant::Dark : ThemeVariant::Light;

        // Colours. Both themes must define the same tokens, so switching
        // themes never leaves a widget without a colour.
        const json& colors = section(root, "color", "color");
        tokens.m_dark = parsePalette(section(colors, "dark", "color.dark"), "dark");
        tokens.m_light = parsePalette(section(colors, "light", "color.light"), "light");
        for (const auto& [name, rgba] : tokens.m_dark) {
            if (!tokens.m_light.contains(name)) {
                throw TokenError("design tokens: colour \"" + name +
                                 "\" exists in the dark theme but not in the light one");
            }
        }
        for (const auto& [name, rgba] : tokens.m_light) {
            if (!tokens.m_dark.contains(name)) {
                throw TokenError("design tokens: colour \"" + name +
                                 "\" exists in the light theme but not in the dark one");
            }
        }

        // Logo colours (rings and pass), per theme.
        const json& logo = section(root, "logo", "logo");
        tokens.m_logoDark = parsePalette(section(logo, "dark", "logo.dark"), "dark");
        tokens.m_logoLight = parsePalette(section(logo, "light", "logo.light"), "light");
        for (const char* part : {"rings", "pass"}) {
            if (!tokens.m_logoDark.contains(part) || !tokens.m_logoLight.contains(part)) {
                throw TokenError(std::string("design tokens: logo.dark and logo.light need \"") +
                                 part + "\"");
            }
        }

        // Type scale.
        const json& scale =
            section(section(root, "typography", "typography"), "scale", "typography.scale");
        for (const auto& [role, value] : scale.items()) {
            tokens.m_typeScale.emplace(role, parseTypeStyle(role, value));
        }

        tokens.m_sizes = numbers(section(root, "size", "size"));
        tokens.m_spacing = numbers(section(root, "spacing", "spacing"));
        tokens.m_radius = numbers(section(root, "radius", "radius"));
        tokens.m_border = numbers(section(root, "border", "border"));

        // Dear ImGui style values: one number or a pair.
        const json& imgui = section(root, "imgui", "imgui");
        for (const auto& [name, value] : section(imgui, "style", "imgui.style").items()) {
            if (value.is_number()) {
                tokens.m_imguiStyle.emplace(name, StyleValue{value.get<float>()});
            } else if (value.is_array() && value.size() == 2 && value[0].is_number() &&
                       value[1].is_number()) {
                tokens.m_imguiStyle.emplace(
                    name, StyleValue{value[0].get<float>(), value[1].get<float>()});
            } else {
                throw TokenError("design tokens: imgui.style." + name +
                                 " must be a number or a pair of numbers");
            }
        }

        // ImGuiCol mapping. Every entry must resolve in both themes.
        for (const auto& [name, value] : section(imgui, "colors", "imgui.colors").items()) {
            if (!value.is_string()) {
                throw TokenError("design tokens: imgui.colors." + name + " must be a string");
            }
            tokens.m_imguiColors.emplace(name, value.get<std::string>());
        }
        for (const auto& [name, expression] : tokens.m_imguiColors) {
            for (ThemeVariant variant : {ThemeVariant::Dark, ThemeVariant::Light}) {
                if (!tokens.resolveColor(variant, expression)) {
                    throw TokenError("design tokens: imgui.colors." + name + " = \"" + expression +
                                     "\" names no colour in the " +
                                     std::string(variantName(variant)) + " theme");
                }
            }
        }

        // Waterfall ramp: exactly 256 colours.
        const json& waterfall = section(root, "waterfall", "waterfall");
        auto lut = waterfall.find("lut256");
        if (lut == waterfall.end() || !lut->is_array() || lut->size() != 256) {
            throw TokenError("design tokens: waterfall.lut256 must list exactly 256 colours");
        }
        for (size_t i = 0; i < 256; ++i) {
            const auto& entry = (*lut)[i];
            auto rgba = entry.is_string() ? parseHexColor(entry.get<std::string>()) : std::nullopt;
            if (!rgba) {
                throw TokenError("design tokens: waterfall.lut256[" + std::to_string(i) +
                                 "] is not #RRGGBB");
            }
            tokens.m_waterfallLut[i] = *rgba;
        }
    } catch (const json::exception& e) {
        // Wrong value types (a string where a number belongs, and so on).
        throw TokenError(std::string("design tokens: ") + e.what());
    }
    return tokens;
}

std::optional<Rgba> DesignTokens::color(ThemeVariant variant, std::string_view token) const {
    const auto& map = palette(variant);
    auto it = map.find(token);
    if (it == map.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Rgba> DesignTokens::resolveColor(ThemeVariant variant,
                                               std::string_view expression) const {
    expression = trim(expression);
    if (expression == "transparent") {
        return Rgba{0.0F, 0.0F, 0.0F, 0.0F};
    }
    if (!expression.empty() && expression.front() == '#') {
        // "#RRGGBB" or "#RRGGBB @NN%".
        const auto at = expression.find('@');
        auto rgba = parseHexColor(trim(expression.substr(0, at)));
        if (!rgba) {
            return std::nullopt;
        }
        if (at != std::string_view::npos) {
            std::string_view percent = trim(expression.substr(at + 1));
            if (percent.empty() || percent.back() != '%') {
                return std::nullopt;
            }
            percent.remove_suffix(1);
            int value = -1;
            auto [end, error] =
                std::from_chars(percent.data(), percent.data() + percent.size(), value);
            if (error != std::errc{} || end != percent.data() + percent.size() || value < 0 ||
                value > 100) {
                return std::nullopt;
            }
            rgba->a = static_cast<float>(value) / 100.0F;
        }
        return rgba;
    }
    return color(variant, expression);
}

std::optional<Rgba> DesignTokens::logoColor(ThemeVariant variant, std::string_view part) const {
    const auto& map = variant == ThemeVariant::Dark ? m_logoDark : m_logoLight;
    auto it = map.find(part);
    if (it == map.end()) {
        return std::nullopt;
    }
    return it->second;
}

const std::map<std::string, Rgba, std::less<>>& DesignTokens::palette(ThemeVariant variant) const {
    return variant == ThemeVariant::Dark ? m_dark : m_light;
}

const TypeStyle* DesignTokens::typeStyle(std::string_view role) const {
    auto it = m_typeScale.find(role);
    return it == m_typeScale.end() ? nullptr : &it->second;
}

std::optional<float> DesignTokens::size(std::string_view name) const {
    return lookup(m_sizes, name);
}

std::optional<float> DesignTokens::spacing(std::string_view step) const {
    return lookup(m_spacing, step);
}

std::optional<float> DesignTokens::radius(std::string_view name) const {
    return lookup(m_radius, name);
}

std::optional<float> DesignTokens::border(std::string_view name) const {
    return lookup(m_border, name);
}

}  // namespace zarescope::theme
