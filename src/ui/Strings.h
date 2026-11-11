// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every piece of UI text, in English (the source) and Romanian.
//
// Widget code never writes a user-visible string literal; it asks for a
// StringId. Adding a string means one new enum value and one new row in
// Strings.cpp with both languages. Strings with values use std::format
// placeholders ("{}"), filled by trf(). Romanian runs about 30 % longer, so
// layouts must let labels grow (design/docs/DESIGN.md §7). Romanian text uses
// the comma-below letters Ș ș Ț ț, never the cedilla forms Ş ş Ţ ţ; a test
// checks that.

#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace zarescope::ui {

enum class Language { English, Romanian };

enum class StringId {
    // Empty state
    EmptyTitle,
    EmptyBody,
    ConnectReceiver,
    OpenRecording,
    // Top bar
    ReceiverChipNone,
    ReceiverChipOpening,
    ChipLive,
    ChipReplay,
    ThemeLabel,
    ThemeDark,
    ThemeLight,
    GainLabel,
    GainAuto,
    GainAutomaticTuner,
    Record,
    StopRecording,
    SettingsLabel,
    LanguageLabel,
    ModeLabel,
    ModeOff,
    AudioLabel,
    VolumeLabel,
    SquelchLabel,
    SquelchOffValue,
    SignalLevel,
    BandwidthLabel,
    DeemphasisLabel,
    Deemphasis50,
    Deemphasis75,
    NoSoundOutput,
    PresetsLabel,
    PresetFmBroadcast,
    PresetAirband,
    PresetAmateur2m,
    PresetPmr446,
    PresetAdsb,
    PresetMeteor,
    FrequencyHelp,
    FrequencyFixedHelp,
    // Device panel
    ReceiversTitle,
    Searching,
    NoReceiverFound,
    SearchAgain,
    Open,
    SerialNumber,
    CloseReceiver,
    CloseRecording,
    TunerLabel,
    IdentityConsistent,
    IdentityMismatch,
    IdentityUnverifiable,
    SampleRateLabel,
    PpmLabel,
    BiasTeeLabel,
    BiasTeeWarning,
    BiasTeeTurnOn,
    Cancel,
    RecordingFromFile,
    // Banners
    RecordingBanner,
    RecordingEndedRetune,
    RecordingFailed,
    ReceiverFailed,
    Dismiss,
    // Spectrum
    TabSpectrum,
    FftInfo,
    FftSizeLabel,
    SmoothingLabel,
    WaterfallFloorLabel,
    WaterfallCeilingLabel,
    FitToSignal,
    TuneHint,
    // Status bar
    StatusVersion,
    StatusUtc,
    StatusLocal,
    StatusNoReceiver,
    StatusReplaying,
    StatusDropped,
    Count  // keep last
};

inline constexpr size_t kStringCount = static_cast<size_t>(StringId::Count);

// The text of `id` in `language` (UTF-8, never empty).
std::string_view text(StringId id, Language language);

// The text with its {} placeholders filled in.
template <typename... Args>
std::string textf(StringId id, Language language, Args&&... args) {
    return std::vformat(text(id, language), std::make_format_args(args...));
}

}  // namespace zarescope::ui
