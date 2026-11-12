// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/Guide.h"

#include <cctype>

namespace zarescope::ui {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
        s.remove_suffix(1);
    }
    return s;
}

// Drops the inline marks the page doesn't render.
std::string plain(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '`') {
            continue;
        }
        if (s[i] == '*' && i + 1 < s.size() && s[i + 1] == '*') {
            ++i;
            continue;
        }
        out += s[i];
    }
    return out;
}

// "1. text" → 1 and "text"; 0 when the line isn't a numbered step.
int stepNumber(std::string_view line, std::string_view& rest) {
    size_t i = 0;
    int number = 0;
    while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])) != 0 && i < 3) {
        number = number * 10 + (line[i] - '0');
        ++i;
    }
    if (i == 0 || i + 1 >= line.size() || line[i] != '.' || line[i + 1] != ' ') {
        return 0;
    }
    rest = trim(line.substr(i + 2));
    return number;
}

// "| a | b |" → the cells, trimmed.
std::vector<std::string> cells(std::string_view line) {
    std::vector<std::string> out;
    line = trim(line);
    if (line.starts_with('|')) {
        line.remove_prefix(1);
    }
    if (line.ends_with('|')) {
        line.remove_suffix(1);
    }
    while (true) {
        const size_t bar = line.find('|');
        out.push_back(plain(trim(line.substr(0, bar))));
        if (bar == std::string_view::npos) {
            break;
        }
        line.remove_prefix(bar + 1);
    }
    return out;
}

bool isTableRule(std::string_view line) {
    for (const char c : line) {
        if (c != '|' && c != '-' && c != ':' && c != ' ') {
            return false;
        }
    }
    return true;
}

}  // namespace

Guide parseGuide(std::string_view markdown) {
    Guide guide;
    std::string paragraph;
    std::string pendingId;
    bool inTable = false;
    bool inComment = false;  // inside a <!-- … --> that spans lines

    auto flush = [&] {
        if (!paragraph.empty()) {
            guide.blocks.push_back({GuideBlock::Kind::Paragraph, plain(paragraph), {}, 0, {}});
            paragraph.clear();
        }
    };

    while (!markdown.empty()) {
        const size_t end = markdown.find('\n');
        const std::string_view raw = markdown.substr(0, end);
        markdown = end == std::string_view::npos ? std::string_view{} : markdown.substr(end + 1);
        const std::string_view line = trim(raw);

        if (inComment) {
            inComment = line.find("-->") == std::string_view::npos;
            continue;
        }
        if (line.empty()) {
            flush();
            inTable = false;
            continue;
        }
        if (line.starts_with("<!--")) {
            flush();
            if (line.find("-->") == std::string_view::npos) {
                inComment = true;  // a note over several lines: skip to its end
                continue;
            }
            // "<!-- id: name -->" names the next section; other comments are notes.
            const std::string_view inner = trim(line.substr(4, line.rfind("-->") - 4));
            if (inner.starts_with("id:")) {
                pendingId = std::string(trim(inner.substr(3)));
            }
            continue;
        }
        if (line.starts_with("### ")) {
            flush();
            guide.blocks.push_back(
                {GuideBlock::Kind::Subsection, plain(trim(line.substr(4))), {}, 0, {}});
            continue;
        }
        if (line.starts_with("## ")) {
            flush();
            guide.sections.push_back(guide.blocks.size());
            guide.blocks.push_back(
                {GuideBlock::Kind::Section, plain(trim(line.substr(3))), {}, 0, pendingId});
            pendingId.clear();
            continue;
        }
        if (line.starts_with("# ")) {
            flush();
            guide.blocks.push_back({GuideBlock::Kind::Title, plain(trim(line.substr(2))), {}, 0, {}});
            continue;
        }
        if (line.starts_with('|')) {
            flush();
            if (isTableRule(line)) {
                continue;
            }
            const auto row = cells(line);
            const std::string label = row.empty() ? std::string() : row[0];
            const std::string text = row.size() > 1 ? row[1] : std::string();
            guide.blocks.push_back({inTable ? GuideBlock::Kind::Row : GuideBlock::Kind::TableHeader,
                                    text, label, 0, {}});
            inTable = true;
            continue;
        }
        if (line.starts_with("- ")) {
            flush();
            guide.blocks.push_back({GuideBlock::Kind::Bullet, plain(trim(line.substr(2))), {}, 0, {}});
            continue;
        }
        std::string_view rest;
        if (const int number = stepNumber(line, rest); number > 0) {
            flush();
            guide.blocks.push_back({GuideBlock::Kind::Step, plain(rest), {}, number, {}});
            continue;
        }
        // A continuation of the previous list item, or paragraph text.
        if (paragraph.empty() && !guide.blocks.empty() && raw.starts_with("  ") &&
            (guide.blocks.back().kind == GuideBlock::Kind::Bullet ||
             guide.blocks.back().kind == GuideBlock::Kind::Step)) {
            guide.blocks.back().text += " " + plain(line);
            continue;
        }
        if (!paragraph.empty()) {
            paragraph += ' ';
        }
        paragraph += line;
    }
    flush();
    return guide;
}

}  // namespace zarescope::ui
