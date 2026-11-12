// SPDX-License-Identifier: GPL-3.0-or-later
//
// The user guide (its Markdown parser and the two shipped guides) and the
// menu bar's model. No window: the menu is checked as data.

#include "ui/AppMenuModel.h"
#include "ui/Guide.h"

#include <doctest/doctest.h>

#include <fstream>
#include <set>
#include <sstream>

using namespace zarescope;
using ui::GuideBlock;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void collectItems(const std::vector<platform::MenuItemSpec>& items, std::vector<const platform::MenuItemSpec*>& out) {
    for (const auto& item : items) {
        out.push_back(&item);
        collectItems(item.submenu, out);
    }
}

std::vector<const platform::MenuItemSpec*> allItems(const std::vector<platform::MenuSpec>& menus) {
    std::vector<const platform::MenuItemSpec*> out;
    for (const auto& menu : menus) {
        collectItems(menu.items, out);
    }
    return out;
}

}  // namespace

TEST_CASE("guide: the Markdown subset becomes blocks") {
    const auto guide = ui::parseGuide(R"(<!-- a note
     over two lines -->
# The title

Intro **bold** with `code`
on two lines.

<!-- id: first -->
## First section

- one
- two
  continued

1. step one
2. step two

### A subsection

| Key | Meaning |
|---|---|
| A | the first |
| B | the second |

## Second section
Last words.
)");
    using Kind = GuideBlock::Kind;
    std::vector<Kind> kinds;
    for (const auto& block : guide.blocks) {
        kinds.push_back(block.kind);
    }
    CHECK(kinds == std::vector<Kind>{Kind::Title, Kind::Paragraph, Kind::Section, Kind::Bullet,
                                     Kind::Bullet, Kind::Step, Kind::Step, Kind::Subsection,
                                     Kind::TableHeader, Kind::Row, Kind::Row, Kind::Section,
                                     Kind::Paragraph});
    CHECK(guide.blocks[1].text == "Intro bold with code on two lines.");
    CHECK(guide.blocks[2].id == "first");
    CHECK(guide.blocks[4].text == "two continued");
    CHECK(guide.blocks[6].number == 2);
    CHECK(guide.blocks[9].label == "A");
    CHECK(guide.blocks[9].text == "the first");
    REQUIRE(guide.sections.size() == 2);
    CHECK(guide.blocks[guide.sections[1]].text == "Second section");
    CHECK(guide.blocks[guide.sections[1]].id.empty());
}

TEST_CASE("guide: the English and Romanian guides have the same sections") {
    const auto english = ui::parseGuide(readFile(ZARESCOPE_HELP_DIR "/guide.en.md"));
    const auto romanian = ui::parseGuide(readFile(ZARESCOPE_HELP_DIR "/guide.ro.md"));
    REQUIRE(english.sections.size() >= 10);
    REQUIRE(english.sections.size() == romanian.sections.size());
    for (size_t i = 0; i < english.sections.size(); ++i) {
        const auto& en = english.blocks[english.sections[i]];
        const auto& ro = romanian.blocks[romanian.sections[i]];
        CHECK(en.id == ro.id);
        CHECK_FALSE(en.id.empty());  // every section can be linked to
    }
    for (const auto* guide : {&english, &romanian}) {
        CHECK(guide->blocks.front().kind == GuideBlock::Kind::Title);
        for (const auto& block : guide->blocks) {
            // No Markdown left over: everything the parser didn't know shows as text.
            CHECK(block.text.find("<!--") == std::string::npos);
            CHECK(block.text.find("##") == std::string::npos);
            if (block.kind == GuideBlock::Kind::Row) {
                CHECK_FALSE(block.label.empty());
                CHECK_FALSE(block.text.empty());
            }
        }
    }
    // The app jumps to these.
    bool shortcuts = false;
    for (const size_t index : english.sections) {
        shortcuts = shortcuts || english.blocks[index].id == "shortcuts";
    }
    CHECK(shortcuts);
}

TEST_CASE("menu: every command is in the menu bar, with no shortcut used twice") {
    const auto menus = ui::buildMenus(ui::Language::English, {"FM radio", "Airband (AM)"});
    REQUIRE(menus.size() == 7);
    CHECK(menus.front().role == "app");
    CHECK(menus[5].role == "window");
    CHECK(menus.back().role == "help");

    std::set<int> commands;
    std::set<std::string> shortcuts;
    for (const auto* item : allItems(menus)) {
        if (item->separator) {
            continue;
        }
        CHECK_FALSE(item->title.empty());
        if (item->command != 0) {
            commands.insert(item->command);
        }
        if (!item->key.empty()) {
            const std::string chord = std::string(item->control ? "^" : "") +
                                      (item->option ? "~" : "") + (item->shift ? "+" : "") +
                                      item->key;
            CHECK_MESSAGE(shortcuts.insert(chord).second, "used twice: " << chord);
        }
    }
    for (int c = static_cast<int>(ui::Command::About); c <= static_cast<int>(ui::Command::ReportProblem);
         ++c) {
        CHECK_MESSAGE(commands.contains(c), "command " << c << " has no menu item");
    }
    CHECK(commands.contains(static_cast<int>(ui::Command::PresetFirst)));
    CHECK(commands.contains(static_cast<int>(ui::Command::PresetFirst) + 1));
}

TEST_CASE("menu: Romanian has the same menus and shortcuts, translated") {
    const auto english = ui::buildMenus(ui::Language::English, {"FM radio"});
    const auto romanian = ui::buildMenus(ui::Language::Romanian, {"Radio FM"});
    const auto en = allItems(english);
    const auto ro = allItems(romanian);
    REQUIRE(en.size() == ro.size());
    size_t translated = 0;
    for (size_t i = 0; i < en.size(); ++i) {
        CHECK(en[i]->command == ro[i]->command);
        CHECK(en[i]->key == ro[i]->key);
        translated += en[i]->title != ro[i]->title ? 1 : 0;
    }
    CHECK(translated > en.size() / 2);
    CHECK(english[1].title == "File");
    CHECK(romanian[1].title == "Fișier");
    CHECK(ui::describeMenus(english).find("Open Recording…  [⌘o]") != std::string::npos);
}
