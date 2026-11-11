// SPDX-License-Identifier: GPL-3.0-or-later
//
// The string table and the status bar clock.

#include "ui/Clock.h"
#include "ui/Strings.h"

#include <doctest/doctest.h>

#include <chrono>
#include <string_view>

using zarescope::ui::Language;
using zarescope::ui::StringId;

TEST_CASE("strings: every id has text in both languages") {
    for (size_t i = 0; i < zarescope::ui::kStringCount; ++i) {
        CAPTURE(i);
        const auto id = static_cast<StringId>(i);
        CHECK_FALSE(text(id, Language::English).empty());
        CHECK_FALSE(text(id, Language::Romanian).empty());
    }
    CHECK(text(StringId::ThemeDark, Language::Romanian) == "Întunecată");
}

TEST_CASE("strings: Romanian uses comma-below Ș ș Ț ț, never the cedilla forms") {
    // UTF-8 for Ş ş Ţ ţ (U+015E, U+015F, U+0162, U+0163).
    constexpr std::string_view kCedilla[] = {"\xC5\x9E", "\xC5\x9F", "\xC5\xA2", "\xC5\xA3"};
    for (size_t i = 0; i < zarescope::ui::kStringCount; ++i) {
        const std::string_view romanian = text(static_cast<StringId>(i), Language::Romanian);
        CAPTURE(romanian);
        for (std::string_view cedilla : kCedilla) {
            CHECK(romanian.find(cedilla) == std::string_view::npos);
        }
    }
}

TEST_CASE("clock: UTC is formatted HH:MM:SS, 24-hour") {
    using namespace std::chrono;
    // 2026-11-11 21:07:05.750 UTC
    const sys_days day = year{2026} / November / 11;
    const auto when = day + hours{21} + minutes{7} + seconds{5} + milliseconds{750};
    CHECK(zarescope::ui::formatClock(when, zarescope::ui::ClockZone::Utc) == "21:07:05");
}

TEST_CASE("clock: local time has the same shape") {
    const auto local = zarescope::ui::formatClock(std::chrono::system_clock::now(),
                                                  zarescope::ui::ClockZone::Local);
    REQUIRE(local.size() == 8);
    CHECK(local[2] == ':');
    CHECK(local[5] == ':');
}
