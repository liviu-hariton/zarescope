# SPDX-License-Identifier: GPL-3.0-or-later
#
# Third-party dependencies, downloaded at configure time with FetchContent.
#
# Every archive is pinned to a release and checked against its SHA-256, so a
# build today and a build next year compile the same code. To update one:
# change the URL, run `shasum -a 256` on the new archive, update the hash and
# docs/DEPENDENCIES.md in the same commit.
#
# Nothing here gets Zarescope's warning flags, and include directories are
# marked SYSTEM so their headers don't trigger warnings in our code.

include(FetchContent)

# --- GLFW 3.5.1 (zlib) — window, input, monitors ----------------------------
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.zip
    URL_HASH SHA256=ea79bc5feffc254c87291980c2d0bce9acebb68c4983b79f961dcd2cb8a611a0
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# --- nlohmann/json 3.12.0 (MIT) — design tokens, later settings and catalogs -
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# --- doctest 2.5.3 (MIT) — unit tests ----------------------------------------
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)  # tests/test_main.cpp has main()
FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
    URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# --- Dear ImGui 1.93.0, docking branch (MIT) — the UI ------------------------
# ImGui ships no CMakeLists.txt. SOURCE_SUBDIR points at a folder that doesn't
# exist, so FetchContent only downloads it and we define the targets below.
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0-docking.tar.gz
    URL_HASH SHA256=746c265540bd3aa109570f29492ef4d8f5b421dd9fd0049c35ca35f21496f1a5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR "no-cmake-here")

FetchContent_MakeAvailable(glfw nlohmann_json doctest imgui)

# zs_imgui: the ImGui core plus the GLFW platform backend. Both are portable C++.
add_library(zs_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
target_include_directories(zs_imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
# Fail loudly on APIs ImGui has marked obsolete, so upgrades surface them.
target_compile_definitions(zs_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(zs_imgui PUBLIC glfw)

# zs_imgui_metal: the Metal renderer backend (Objective-C++), macOS only.
if(APPLE)
    add_library(zs_imgui_metal STATIC ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm)
    target_compile_options(zs_imgui_metal PRIVATE -fobjc-arc)
    target_link_libraries(zs_imgui_metal PUBLIC zs_imgui
        "-framework Metal" "-framework QuartzCore")
endif()
