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

# --- ImPlot 1.1 (MIT) — spectrum plot and its frequency axis ----------------
FetchContent_Declare(implot
    URL https://github.com/epezent/implot/archive/refs/tags/v1.1.tar.gz
    URL_HASH SHA256=f13e1e111db2df1687e1568644429c2184eb5115867dc76bb0a8bd17a2e72ae8
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR "no-cmake-here")

# --- KissFFT 131.1.0 (BSD-3-Clause) — FFT, behind dsp::FftEngine -------------
FetchContent_Declare(kissfft
    URL https://github.com/mborgerding/kissfft/archive/refs/tags/131.1.0.tar.gz
    URL_HASH SHA256=76c1aac87ddb7258f34b08a13f0eebf9e53afa299857568346aa5c82bcafaf1a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR "no-cmake-here")

# --- librtlsdr, RTL-SDR Blog fork V1.4.0 (GPL-2.0-or-later) — the receiver ---
# V1.4.0 (2026-08) recognises the Blog V4 (R828D) and the Blog V4 Lite (R828S).
# Built here as a static library from its sources, so it needs no pkg-config.
FetchContent_Declare(rtlsdr
    URL https://github.com/rtlsdrblog/rtl-sdr-blog/archive/refs/tags/V1.4.0.tar.gz
    URL_HASH SHA256=89520433e63b1a1371b45d8deea198cad9dbd1f0d55cc3a706aafb7b187bd325
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR "no-cmake-here")

FetchContent_MakeAvailable(glfw nlohmann_json doctest imgui implot kissfft rtlsdr)

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

# zs_implot: ImPlot on top of zs_imgui.
add_library(zs_implot STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp)
target_include_directories(zs_implot SYSTEM PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(zs_implot PUBLIC zs_imgui)

# zs_kissfft: the complex FFT only (single precision, the default scalar).
add_library(zs_kissfft STATIC ${kissfft_SOURCE_DIR}/kiss_fft.c)
target_include_directories(zs_kissfft SYSTEM PUBLIC ${kissfft_SOURCE_DIR})

# --- libusb (LGPL-2.1-or-later), from Homebrew: `brew install libusb` ---------
# Linked statically so the app bundle carries it (M7). Its static form needs
# the frameworks below (from libusb-1.0.pc, Libs.private).
find_path(ZS_LIBUSB_INCLUDE_DIR libusb.h
    PATH_SUFFIXES libusb-1.0
    HINTS /opt/homebrew/opt/libusb/include /usr/local/opt/libusb/include)
find_library(ZS_LIBUSB_STATIC NAMES libusb-1.0.a
    HINTS /opt/homebrew/opt/libusb/lib /usr/local/opt/libusb/lib)
if(NOT ZS_LIBUSB_INCLUDE_DIR OR NOT ZS_LIBUSB_STATIC)
    message(FATAL_ERROR "libusb not found. Install it with: brew install libusb")
endif()
add_library(zs_libusb INTERFACE)
target_include_directories(zs_libusb SYSTEM INTERFACE ${ZS_LIBUSB_INCLUDE_DIR})
target_link_libraries(zs_libusb INTERFACE ${ZS_LIBUSB_STATIC})
if(APPLE)
    target_link_libraries(zs_libusb INTERFACE
        objc "-framework IOKit" "-framework CoreFoundation" "-framework Security")
endif()

# zs_rtlsdr: librtlsdr with its tuner drivers, static (rtlsdr_STATIC empties
# the export macro).
find_package(Threads REQUIRED)
add_library(zs_rtlsdr STATIC
    ${rtlsdr_SOURCE_DIR}/src/librtlsdr.c
    ${rtlsdr_SOURCE_DIR}/src/tuner_e4k.c
    ${rtlsdr_SOURCE_DIR}/src/tuner_fc0012.c
    ${rtlsdr_SOURCE_DIR}/src/tuner_fc0013.c
    ${rtlsdr_SOURCE_DIR}/src/tuner_fc2580.c
    ${rtlsdr_SOURCE_DIR}/src/tuner_r82xx.c)
target_include_directories(zs_rtlsdr SYSTEM PUBLIC ${rtlsdr_SOURCE_DIR}/include)
target_compile_definitions(zs_rtlsdr PUBLIC rtlsdr_STATIC)
# Upstream C code: keep its warnings out of our build log.
target_compile_options(zs_rtlsdr PRIVATE -w)
target_link_libraries(zs_rtlsdr PUBLIC zs_libusb Threads::Threads)
