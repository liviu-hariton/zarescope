// SPDX-License-Identifier: GPL-3.0-or-later
//
// The string table. See Strings.h.

#include "ui/Strings.h"

#include <array>

namespace zarescope::ui {

namespace {

struct Entry {
    StringId id;
    std::string_view english;
    std::string_view romanian;
};

// One row per StringId, in enum order. The static_assert below fails the
// build if a row is missing or out of place.
constexpr std::array<Entry, kStringCount> kStrings{{
    {StringId::EmptyTitle, "No receiver yet", "Niciun receptor încă"},
    {StringId::EmptyBody,
     "This build doesn't talk to RTL-SDR sticks yet. The live spectrum and waterfall will "
     "appear here once receiver support is in.",
     "Această versiune nu comunică încă cu stickurile RTL-SDR. Spectrul live și waterfall-ul "
     "vor apărea aici după ce se adaugă suportul pentru receptor."},
    {StringId::ReceiverChipNone, "No receiver", "Niciun receptor"},
    {StringId::ThemeLabel, "Theme", "Temă"},
    {StringId::ThemeDark, "Dark", "Întunecată"},
    {StringId::ThemeLight, "Light", "Luminoasă"},
    {StringId::StatusVersion, "Zarescope", "Zarescope"},
    {StringId::StatusUtc, "UTC", "UTC"},
    {StringId::StatusLocal, "Local", "Local"},
}};

constexpr bool rowsMatchIds() {
    for (size_t i = 0; i < kStrings.size(); ++i) {
        if (static_cast<size_t>(kStrings[i].id) != i || kStrings[i].english.empty() ||
            kStrings[i].romanian.empty()) {
            return false;
        }
    }
    return true;
}
static_assert(rowsMatchIds(), "kStrings needs one row per StringId, in enum order, both languages");

}  // namespace

std::string_view text(StringId id, Language language) {
    const Entry& entry = kStrings[static_cast<size_t>(id)];
    return language == Language::Romanian ? entry.romanian : entry.english;
}

}  // namespace zarescope::ui
