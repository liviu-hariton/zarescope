# SPDX-License-Identifier: GPL-3.0-or-later
#
# Unit tests (doctest). None of them needs an RTL-SDR stick, a window or a
# GPU: ImGui runs headless here, which is enough to check the style and the
# fonts. Run with:  ctest --test-dir build --output-on-failure
#
# Tests that need real hardware arrive with M1. They will be tagged
# "[hardware]" and left out of the default run.

add_executable(zarescope_tests
    test_main.cpp
    test_smoke.cpp
    test_design_tokens.cpp
    test_dsp.cpp
    test_imgui_theme.cpp
    test_typography.cpp
    test_strings_clock.cpp)
target_link_libraries(zarescope_tests PRIVATE zarescope_ui zarescope_dsp doctest::doctest)
target_compile_definitions(zarescope_tests PRIVATE
    ZARESCOPE_VERSION="${PROJECT_VERSION}"
    ZARESCOPE_TOKENS_FILE="${ZARESCOPE_TOKENS_FILE}"
    ZARESCOPE_LUT_CSV="${PROJECT_SOURCE_DIR}/design/tokens/waterfall-lut.csv"
    ZARESCOPE_FONTS_DIR="${ZARESCOPE_FONTS_DIR}")
zarescope_set_warnings(zarescope_tests)

# One CTest test per doctest test case, so failures are reported by name.
include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
doctest_discover_tests(zarescope_tests)
