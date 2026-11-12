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

# --- miniaudio 0.11.26 (public domain / MIT-0) — sound output -----------------
FetchContent_Declare(miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.26.tar.gz
    URL_HASH SHA256=838ba55a21c9142adf7b76bdc4351e5cd8b7ae128f46b25ec25f94ae9969c600
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR "no-cmake-here")

# --- sgp4 v3.0 (Apache-2.0) — satellite orbits from TLEs ----------------------
FetchContent_Declare(sgp4
    URL https://github.com/dnwrnr/sgp4/archive/refs/tags/v3.0.tar.gz
    URL_HASH SHA256=53a1ab9c2ce860af15dd8397cb4a7a1904560d72c7a78c748a34ce2755b0f301
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR "no-cmake-here")

FetchContent_MakeAvailable(glfw nlohmann_json doctest imgui implot kissfft rtlsdr miniaudio sgp4)

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

# --- libusb 1.0.30 (LGPL-2.1-or-later) — USB access for librtlsdr ------------
# The libusb project's own CMake build (libusb-cmake), which carries the
# libusb sources. Built here, static, for our deployment target: Homebrew's
# libusb-1.0.a is built for the newest macOS only, which would break the
# bundle on older systems (M7).
set(LIBUSB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBUSB_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libusb
    URL https://github.com/libusb/libusb-cmake/archive/refs/tags/v1.0.30-0.tar.gz
    URL_HASH SHA256=c8143ceba42b7b93d484ccd09db7be261bb9b15f61dae5fdc605a01a6913bada
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)
FetchContent_MakeAvailable(libusb)
# Upstream C code: keep its warnings out of our build log.
target_compile_options(usb-1.0 PRIVATE -w)

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
target_link_libraries(zs_rtlsdr PUBLIC usb-1.0 Threads::Threads)

# zs_miniaudio: playback only. Decoders, encoders, the node graph and the
# engine are left out; the system audio frameworks are linked directly
# instead of loaded at run time.
add_library(zs_miniaudio STATIC ${miniaudio_SOURCE_DIR}/miniaudio.c)
target_include_directories(zs_miniaudio SYSTEM PUBLIC ${miniaudio_SOURCE_DIR})
target_compile_definitions(zs_miniaudio PUBLIC
    MA_NO_DECODING MA_NO_ENCODING MA_NO_GENERATION MA_NO_RESOURCE_MANAGER
    MA_NO_NODE_GRAPH MA_NO_ENGINE MA_NO_RUNTIME_LINKING)
target_compile_options(zs_miniaudio PRIVATE -w)
if(APPLE)
    target_link_libraries(zs_miniaudio PUBLIC
        "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
endif()

# zs_sgp4: the SGP4/SDP4 propagator (namespace libsgp4).
file(GLOB ZS_SGP4_SOURCES ${sgp4_SOURCE_DIR}/libsgp4/*.cpp)
add_library(zs_sgp4 STATIC ${ZS_SGP4_SOURCES})
target_include_directories(zs_sgp4 SYSTEM PUBLIC ${sgp4_SOURCE_DIR}/libsgp4)
target_compile_options(zs_sgp4 PRIVATE -w)

# --- libcurl (the system's) — TLE downloads from CelesTrak --------------------
# Part of macOS; on Linux and Windows a system or vcpkg package.
find_package(CURL REQUIRED)

# --- SQLite (the system's) — the journal (M6) ---------------------------------
# Part of macOS (libsqlite3 in the SDK); on Linux and Windows a system or
# vcpkg package.
find_package(SQLite3 REQUIRED)
