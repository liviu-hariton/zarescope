// SPDX-License-Identifier: GPL-3.0-or-later
//
// The simplest test: the runner works and the build passes the version in.

#include <doctest/doctest.h>

#include <string_view>

TEST_CASE("smoke: the version is MAJOR.MINOR.PATCH") {
    constexpr std::string_view version = ZARESCOPE_VERSION;
    int dots = 0;
    for (char c : version) {
        if (c == '.') {
            ++dots;
        } else {
            CHECK(c >= '0');
            CHECK(c <= '9');
        }
    }
    CHECK(dots == 2);
}
