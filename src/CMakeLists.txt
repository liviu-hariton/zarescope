# SPDX-License-Identifier: GPL-3.0-or-later
#
# Zarescope's libraries. Each one is small and has a clear dependency
# direction, so the parts without a UI can be tested on their own:
#
#   zarescope_dsp       FFT, spectrum, queues    → KissFFT
#   zarescope_adsb      Mode S / ADS-B decoder   → zarescope_dsp
#   zarescope_audio     sound output             → miniaudio
#   zarescope_device    sample sources, IQ files → zarescope_dsp, librtlsdr
#   zarescope_theme     design tokens            → nlohmann/json
#   zarescope_ui        ImGui screens and style  → zarescope_theme, Dear ImGui
#   zarescope_platform  Metal, macOS paths       → Dear ImGui's Metal backend
#   Zarescope (app)     main() and the loop      → all of the above

add_library(zarescope_dsp STATIC
    dsp/AudioDemodulator.cpp
    dsp/Filters.cpp
    dsp/IqConvert.cpp
    dsp/KissFftEngine.cpp
    dsp/SignalGenerator.cpp
    dsp/SpectrumAnalyzer.cpp
    dsp/Window.cpp)
target_include_directories(zarescope_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zarescope_dsp PRIVATE zs_kissfft)
zarescope_set_warnings(zarescope_dsp)

add_library(zarescope_audio STATIC
    audio/AudioOutput.cpp)
target_include_directories(zarescope_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zarescope_audio PRIVATE zs_miniaudio)
zarescope_set_warnings(zarescope_audio)

add_library(zarescope_device STATIC
    device/DeviceIdentity.cpp
    device/FileSource.cpp
    device/IqMetadata.cpp
    device/IqRecorder.cpp
    device/Presets.cpp
    device/Receiver.cpp
    device/RtlSdrSource.cpp
    device/SampleSource.cpp)
target_link_libraries(zarescope_device
    PUBLIC zarescope_dsp zarescope_audio zarescope_adsb
    PRIVATE nlohmann_json::nlohmann_json zs_rtlsdr)
zarescope_set_warnings(zarescope_device)

add_library(zarescope_adsb STATIC
    decoders/adsb/AircraftStore.cpp
    decoders/adsb/Cpr.cpp
    decoders/adsb/Demodulator.cpp
    decoders/adsb/Encoder.cpp
    decoders/adsb/ModeS.cpp)
target_include_directories(zarescope_adsb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zarescope_adsb PUBLIC zarescope_dsp)
zarescope_set_warnings(zarescope_adsb)

add_library(zarescope_theme STATIC
    ui/theme/DesignTokens.cpp)
target_include_directories(zarescope_theme PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zarescope_theme PUBLIC nlohmann_json::nlohmann_json)
zarescope_set_warnings(zarescope_theme)

add_library(zarescope_ui STATIC
    ui/theme/ImGuiTheme.cpp
    ui/theme/PlotTheme.cpp
    ui/Clock.cpp
    ui/Format.cpp
    ui/FrequencyDisplay.cpp
    ui/LogoMark.cpp
    ui/MainWindow.cpp
    ui/SpectrumView.cpp
    ui/Strings.cpp
    ui/Typography.cpp
    ui/UiKit.cpp
    ui/WaterfallColors.cpp)
target_link_libraries(zarescope_ui PUBLIC zarescope_theme zarescope_device zs_imgui zs_implot)
zarescope_set_warnings(zarescope_ui)

if(APPLE)
    add_library(zarescope_platform STATIC
        platform/macos/MacAppPaths.mm
        platform/macos/MacFileDialog.mm
        platform/macos/MacWindowAppearance.mm
        platform/macos/MetalRenderer.mm)
    target_compile_options(zarescope_platform PRIVATE -fobjc-arc)
    target_include_directories(zarescope_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(zarescope_platform
        PUBLIC zs_imgui_metal
        PRIVATE "-framework Cocoa" "-framework Foundation" "-framework UniformTypeIdentifiers")
    zarescope_set_warnings(zarescope_platform)
else()
    message(FATAL_ERROR "Zarescope v1 builds on macOS (Apple Silicon) only; Linux and Windows are later targets")
endif()

add_subdirectory(app)
