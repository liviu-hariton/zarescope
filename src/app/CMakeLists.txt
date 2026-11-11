# SPDX-License-Identifier: GPL-3.0-or-later
#
# Zarescope.app: the macOS application bundle.
#
#   Contents/MacOS/Zarescope                 the executable
#   Contents/Resources/zarescope.tokens.json the design tokens
#   Contents/Resources/fonts/                Inter, JetBrains Mono and their licences
#   Contents/Resources/Zarescope.icns        built from design/brand/app-icon
#
# The bundle lands at build/Zarescope.app.

# The app icon, built from the iconset with macOS's iconutil at build time.
set(ZARESCOPE_ICONSET "${PROJECT_SOURCE_DIR}/design/brand/app-icon/Zarescope.iconset")
set(ZARESCOPE_ICNS "${CMAKE_CURRENT_BINARY_DIR}/Zarescope.icns")
file(GLOB ZARESCOPE_ICONSET_PNGS CONFIGURE_DEPENDS "${ZARESCOPE_ICONSET}/*.png")
add_custom_command(
    OUTPUT "${ZARESCOPE_ICNS}"
    COMMAND iconutil -c icns -o "${ZARESCOPE_ICNS}" "${ZARESCOPE_ICONSET}"
    DEPENDS ${ZARESCOPE_ICONSET_PNGS}
    COMMENT "Building Zarescope.icns from the iconset"
    VERBATIM)
set_source_files_properties("${ZARESCOPE_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

set_source_files_properties("${ZARESCOPE_TOKENS_FILE}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

file(GLOB ZARESCOPE_FONT_FILES CONFIGURE_DEPENDS
    "${ZARESCOPE_FONTS_DIR}/*.ttf"
    "${ZARESCOPE_FONTS_DIR}/*-OFL.txt")
set_source_files_properties(${ZARESCOPE_FONT_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts")

add_executable(Zarescope MACOSX_BUNDLE
    main.cpp
    App.h App.cpp
    "${ZARESCOPE_ICNS}"
    "${ZARESCOPE_TOKENS_FILE}"
    ${ZARESCOPE_FONT_FILES})

# The year of the build, for the copyright line. CMake honours
# SOURCE_DATE_EPOCH here, so reproducible builds stay reproducible.
string(TIMESTAMP ZARESCOPE_BUILD_YEAR "%Y" UTC)

set_target_properties(Zarescope PROPERTIES
    OUTPUT_NAME "Zarescope"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/cmake/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "eu.zarescope.Zarescope"
    MACOSX_BUNDLE_BUNDLE_NAME "Zarescope"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "Zarescope.icns"
    MACOSX_BUNDLE_COPYRIGHT "© ${ZARESCOPE_BUILD_YEAR} Liviu Hariton. GPL-3.0-or-later.")

target_compile_definitions(Zarescope PRIVATE ZARESCOPE_VERSION="${PROJECT_VERSION}")
target_link_libraries(Zarescope PRIVATE zarescope_ui zarescope_platform)
zarescope_set_warnings(Zarescope)
