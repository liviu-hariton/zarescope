# SPDX-License-Identifier: GPL-3.0-or-later
#
# Unit tests (doctest). None of them needs an RTL-SDR stick, a window or a
# GPU: ImGui runs headless here, which is enough to check the style and the
# fonts. Run with:  ctest --test-dir build --output-on-failure
#
# Tests that need an RTL-SDR stick are named "[hardware] …" and live in their
# own executable, zarescope_hardware_tests, which CTest never runs
# (hardware tests never run by default). Run it by hand with the stick plugged in.

add_executable(zarescope_tests
    test_main.cpp
    test_smoke.cpp
    test_adsb.cpp
    test_audio.cpp
    test_design_tokens.cpp
    test_dsp.cpp
    test_imgui_theme.cpp
    test_iq_files.cpp
    test_presets.cpp
    test_receiver.cpp
    test_sat.cpp
    test_typography.cpp
    test_ui_format.cpp
    test_strings_clock.cpp)
target_link_libraries(zarescope_tests PRIVATE zarescope_ui zarescope_device zarescope_adsb zarescope_sat doctest::doctest)
target_compile_definitions(zarescope_tests PRIVATE
    ZARESCOPE_VERSION="${PROJECT_VERSION}"
    ZARESCOPE_TOKENS_FILE="${ZARESCOPE_TOKENS_FILE}"
    ZARESCOPE_LUT_CSV="${PROJECT_SOURCE_DIR}/design/tokens/waterfall-lut.csv"
    ZARESCOPE_FONTS_DIR="${ZARESCOPE_FONTS_DIR}"
    ZARESCOPE_PRESETS_FILE="${PROJECT_SOURCE_DIR}/resources/presets.json"
    ZARESCOPE_SAT_CATALOG_FILE="${PROJECT_SOURCE_DIR}/resources/sat_catalog.json")
# The golden pass list for test_sat.cpp, kept as a data file.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/data/golden_passes.txt" ZARESCOPE_GOLDEN_PASSES)
string(REPLACE "\n" "\\n" ZARESCOPE_GOLDEN_PASSES_ESCAPED "${ZARESCOPE_GOLDEN_PASSES}")
target_compile_definitions(zarescope_tests PRIVATE
    ZARESCOPE_GOLDEN_PASSES="${ZARESCOPE_GOLDEN_PASSES_ESCAPED}")
zarescope_set_warnings(zarescope_tests)

# One CTest test per doctest test case, so failures are reported by name.
include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
doctest_discover_tests(zarescope_tests)

add_executable(zarescope_hardware_tests
    test_main.cpp
    hardware/test_hardware.cpp)
target_link_libraries(zarescope_hardware_tests PRIVATE zarescope_device doctest::doctest)
zarescope_set_warnings(zarescope_hardware_tests)
