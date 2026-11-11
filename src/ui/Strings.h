// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every piece of UI text, in English (the source) and Romanian.
//
// Widget code never writes a user-visible string literal; it asks for a
// StringId. Adding a string means one new enum value and one new row in
// Strings.cpp with both languages. Romanian runs about 30 % longer, so
// layouts must let labels grow (design/docs/DESIGN.md §7). Romanian text uses
// the comma-below letters Ș ș Ț ț, never the cedilla forms Ş ş Ţ ţ; a test
// checks that.

#pragma once

#include <cstddef>
#include <string_view>

namespace zarescope::ui {

enum class Language { English, Romanian };

enum class StringId {
    EmptyTitle,
    EmptyBody,
    ReceiverChipNone,
    ThemeLabel,
    ThemeDark,
    ThemeLight,
    StatusVersion,
    StatusUtc,
    StatusLocal,
    Count  // keep last
};

inline constexpr size_t kStringCount = static_cast<size_t>(StringId::Count);

// The text of `id` in `language` (UTF-8, never empty).
std::string_view text(StringId id, Language language);

}  // namespace zarescope::ui
