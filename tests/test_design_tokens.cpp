// SPDX-License-Identifier: GPL-3.0-or-later
//
// DesignTokens: the real token file loads and is self-consistent, broken
// files are refused with a clear error, and the colour pairs the UI draws
// meet WCAG AA.

#include "ui/theme/DesignTokens.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

using zarescope::theme::DesignTokens;
using zarescope::theme::parseHexColor;
using zarescope::theme::Rgba;
using zarescope::theme::ThemeVariant;
using zarescope::theme::TokenError;

namespace {

std::string readFile(const char* path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

nlohmann::json realTokensJson() { return nlohmann::json::parse(readFile(ZARESCOPE_TOKENS_FILE)); }

// WCAG 2.x relative luminance and contrast ratio.
double channel(float c) { return c <= 0.04045F ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
double luminance(const Rgba& c) {
    return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
}
double contrast(const Rgba& a, const Rgba& b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

}  // namespace

TEST_CASE("tokens: hex colours parse, everything else is refused") {
    auto amber = parseHexColor("#FFB547");
    REQUIRE(amber);
    CHECK(amber->r == doctest::Approx(1.0));
    CHECK(amber->g == doctest::Approx(0xB5 / 255.0));
    CHECK(amber->b == doctest::Approx(0x47 / 255.0));
    CHECK(amber->a == doctest::Approx(1.0));

    auto translucent = parseHexColor("#00000080");
    REQUIRE(translucent);
    CHECK(translucent->a == doctest::Approx(0x80 / 255.0));

    CHECK(parseHexColor("#ffb547") == parseHexColor("#FFB547"));
    CHECK_FALSE(parseHexColor(""));
    CHECK_FALSE(parseHexColor("FFB547"));
    CHECK_FALSE(parseHexColor("#FFB54"));
    CHECK_FALSE(parseHexColor("#GGB547"));
    CHECK_FALSE(parseHexColor("#FFB5477"));
}

TEST_CASE("tokens: the design package's token file loads") {
    const DesignTokens tokens = DesignTokens::fromFile(ZARESCOPE_TOKENS_FILE);

    CHECK(tokens.version() == "1.0.0");
    CHECK(tokens.defaultVariant() == ThemeVariant::Dark);

    // A few anchors from design/docs/DESIGN.md §2.
    CHECK(tokens.color(ThemeVariant::Dark, "accent") == parseHexColor("#FFB547"));
    CHECK(tokens.color(ThemeVariant::Light, "accent") == parseHexColor("#9A5B00"));
    CHECK(tokens.color(ThemeVariant::Dark, "bg/base") == parseHexColor("#080B14"));
    CHECK(tokens.color(ThemeVariant::Light, "bg/surface") == parseHexColor("#FFFFFF"));
    CHECK(tokens.color(ThemeVariant::Dark, "pass-imminent") == parseHexColor("#A99BFF"));
    CHECK_FALSE(tokens.color(ThemeVariant::Dark, "no-such-token"));

    // The light logo amber is a graphics-only colour, not the accent.
    CHECK(tokens.logoColor(ThemeVariant::Light, "pass") == parseHexColor("#C06A00"));
    CHECK(tokens.logoColor(ThemeVariant::Dark, "rings") ==
          tokens.color(ThemeVariant::Dark, "text/primary"));

    const auto* body = tokens.typeStyle("body");
    REQUIRE(body != nullptr);
    CHECK(body->sizePx == doctest::Approx(13.0));
    CHECK(body->lineHeightPx == doctest::Approx(18.0));
    CHECK(body->weight == 400);
    const auto* caption = tokens.typeStyle("caption");
    REQUIRE(caption != nullptr);
    CHECK(caption->uppercase);
    const auto* freq = tokens.typeStyle("mono/freq");
    REQUIRE(freq != nullptr);
    CHECK(freq->family == zarescope::theme::TypeStyle::Family::Mono);

    CHECK(tokens.size("topBar") == 64.0F);
    CHECK(tokens.size("statusBar") == 28.0F);
    CHECK(tokens.spacing("16") == 16.0F);
    CHECK(tokens.radius("md") == 6.0F);
    CHECK(tokens.border("focus") == 2.0F);
}

TEST_CASE("tokens: both themes define the same colour tokens") {
    const DesignTokens tokens = DesignTokens::fromFile(ZARESCOPE_TOKENS_FILE);
    const auto& dark = tokens.palette(ThemeVariant::Dark);
    const auto& light = tokens.palette(ThemeVariant::Light);
    CHECK(dark.size() == light.size());
    for (const auto& [name, rgba] : dark) {
        CAPTURE(name);
        CHECK(light.contains(name));
    }
}

TEST_CASE("tokens: imgui colour expressions resolve") {
    const DesignTokens tokens = DesignTokens::fromFile(ZARESCOPE_TOKENS_FILE);
    CHECK(tokens.resolveColor(ThemeVariant::Dark, "transparent") == Rgba{0, 0, 0, 0});
    auto dim = tokens.resolveColor(ThemeVariant::Dark, "#000000 @50%");
    REQUIRE(dim);
    CHECK(dim->a == doctest::Approx(0.5));
    CHECK(tokens.resolveColor(ThemeVariant::Light, "accent") ==
          tokens.color(ThemeVariant::Light, "accent"));
    CHECK_FALSE(tokens.resolveColor(ThemeVariant::Dark, "#000000 @150%"));
    CHECK_FALSE(tokens.resolveColor(ThemeVariant::Dark, "#000000 @50"));
    CHECK_FALSE(tokens.resolveColor(ThemeVariant::Dark, "nonsense"));
}

TEST_CASE("tokens: the waterfall LUT matches waterfall-lut.csv and gets brighter") {
    const DesignTokens tokens = DesignTokens::fromFile(ZARESCOPE_TOKENS_FILE);
    const auto& lut = tokens.waterfallLut();

    std::istringstream csv(readFile(ZARESCOPE_LUT_CSV));
    std::string line;
    std::getline(csv, line);  // header: index,hex
    int rows = 0;
    while (std::getline(csv, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const auto comma = line.find(',');
        REQUIRE(comma != std::string::npos);
        const int index = std::stoi(line.substr(0, comma));
        REQUIRE(index >= 0);
        REQUIRE(index < 256);
        CAPTURE(index);
        CHECK(parseHexColor(line.substr(comma + 1)) == lut[static_cast<size_t>(index)]);
        ++rows;
    }
    CHECK(rows == 256);

    // DESIGN.md §2: lightness rises monotonically from floor to ceiling, so
    // the ramp reads correctly for colour-blind users.
    for (size_t i = 1; i < lut.size(); ++i) {
        CAPTURE(i);
        CHECK(luminance(lut[i]) >= luminance(lut[i - 1]) - 1e-6);
    }
}

TEST_CASE("tokens: broken token files are refused") {
    SUBCASE("not JSON") { CHECK_THROWS_AS(DesignTokens::fromJson("{ nope"), TokenError); }
    SUBCASE("missing file") {
        CHECK_THROWS_AS(DesignTokens::fromFile("/nonexistent/zarescope.tokens.json"), TokenError);
    }
    SUBCASE("a colour only one theme has") {
        auto json = realTokensJson();
        json["color"]["dark"]["extra"] = "#123456";
        CHECK_THROWS_WITH_AS(DesignTokens::fromJson(json.dump()),
                             doctest::Contains("not in the light one"), TokenError);
    }
    SUBCASE("a colour that isn't hex") {
        auto json = realTokensJson();
        json["color"]["light"]["accent"] = "orange";
        CHECK_THROWS_AS(DesignTokens::fromJson(json.dump()), TokenError);
    }
    SUBCASE("an imgui colour naming a token that doesn't exist") {
        auto json = realTokensJson();
        json["imgui"]["colors"]["Text"] = "text/tertiary";
        CHECK_THROWS_WITH_AS(DesignTokens::fromJson(json.dump()),
                             doctest::Contains("names no colour"), TokenError);
    }
    SUBCASE("a short waterfall LUT") {
        auto json = realTokensJson();
        json["waterfall"]["lut256"].erase(0);
        CHECK_THROWS_WITH_AS(DesignTokens::fromJson(json.dump()), doctest::Contains("256"),
                             TokenError);
    }
    SUBCASE("a type style with a wrong value type") {
        auto json = realTokensJson();
        json["typography"]["scale"]["body"]["size"] = "big";
        CHECK_THROWS_AS(DesignTokens::fromJson(json.dump()), TokenError);
    }
}

TEST_CASE("tokens: the colour pairs the shell draws meet WCAG AA") {
    const DesignTokens tokens = DesignTokens::fromFile(ZARESCOPE_TOKENS_FILE);
    struct Pair {
        const char* foreground;
        const char* background;
        double minimum;  // 4.5 for text, 3.0 for UI graphics
    };
    const Pair pairs[] = {
        {"text/primary", "bg/base", 4.5},       // empty-state title
        {"text/secondary", "bg/base", 4.5},     // empty-state paragraph
        {"text/primary", "bg/surface", 4.5},    // status bar values
        {"text/secondary", "bg/surface", 4.5},  // status bar labels
        {"text/secondary", "bg/raised", 4.5},   // chip label, unselected segment
        {"text/primary", "bg/hover", 4.5},      // hovered segment
        {"text/primary", "accent/soft", 4.5},   // selected segment
        {"on-accent", "accent", 4.5},           // primary buttons
        {"accent", "bg/surface", 3.0},          // logo track, focus ring
        // Input borders against the panel around the field (WCAG 1.4.11
        // compares a boundary with its adjacent colour).
        {"border/input", "bg/surface", 3.0},
    };
    for (ThemeVariant variant : {ThemeVariant::Dark, ThemeVariant::Light}) {
        for (const Pair& pair : pairs) {
            CAPTURE(zarescope::theme::variantName(variant));
            CAPTURE(pair.foreground);
            CAPTURE(pair.background);
            auto fg = tokens.color(variant, pair.foreground);
            auto bg = tokens.color(variant, pair.background);
            REQUIRE(fg);
            REQUIRE(bg);
            CHECK(contrast(*fg, *bg) >= pair.minimum);
        }
    }
}
