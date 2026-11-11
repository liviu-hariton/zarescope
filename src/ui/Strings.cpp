// SPDX-License-Identifier: GPL-3.0-or-later
//
// The string table. See Strings.h.

#include "ui/Strings.h"

#include <array>

namespace zarescope::ui {

namespace {

struct Entry {
    StringId id;
    std::string_view english;
    std::string_view romanian;
};

// One row per StringId, in enum order. The static_assert below fails the
// build if a row is missing or out of place.
constexpr std::array<Entry, kStringCount> kStrings{{
    // Empty state
    {StringId::EmptyTitle, "Nothing to show yet", "Nimic de afișat încă"},
    {StringId::EmptyBody, "Connect an RTL-SDR stick to see the live spectrum, or open a recording.",
     "Conectează un stick RTL-SDR ca să vezi spectrul live sau deschide o înregistrare."},
    {StringId::ConnectReceiver, "Connect receiver", "Conectează receptorul"},
    {StringId::OpenRecording, "Open recording…", "Deschide o înregistrare…"},
    // Top bar
    {StringId::ReceiverChipNone, "No receiver", "Niciun receptor"},
    {StringId::ReceiverChipOpening, "Opening…", "Se deschide…"},
    {StringId::ChipLive, "LIVE", "LIVE"},
    {StringId::ChipReplay, "REPLAY", "REDARE"},
    {StringId::ThemeLabel, "Theme", "Temă"},
    {StringId::ThemeDark, "Dark", "Întunecată"},
    {StringId::ThemeLight, "Light", "Luminoasă"},
    {StringId::GainLabel, "Gain", "Câștig"},
    {StringId::GainAuto, "Auto", "Automat"},
    {StringId::GainAutomaticTuner, "Automatic (the tuner decides)", "Automat (decide tunerul)"},
    {StringId::Record, "Record", "Înregistrează"},
    {StringId::StopRecording, "Stop recording", "Oprește înregistrarea"},
    {StringId::SettingsLabel, "Settings", "Setări"},
    {StringId::LanguageLabel, "Language", "Limbă"},
    {StringId::ModeLabel, "How the signal becomes sound (demodulation)",
     "Cum devine semnalul sunet (demodulare)"},
    {StringId::ModeOff, "Off", "Oprit"},
    {StringId::AudioLabel, "Sound", "Sunet"},
    {StringId::VolumeLabel, "Volume", "Volum"},
    {StringId::SquelchLabel, "Squelch: silence below this signal level",
     "Squelch: liniște sub acest nivel de semnal"},
    {StringId::SquelchOffValue, "off", "oprit"},
    {StringId::SignalLevel, "Signal in the channel: {}", "Semnal în canal: {}"},
    {StringId::BandwidthLabel, "Channel width", "Lățimea canalului"},
    {StringId::DeemphasisLabel, "De-emphasis", "De-emfază"},
    {StringId::Deemphasis50, "50 µs (Europe)", "50 µs (Europa)"},
    {StringId::Deemphasis75, "75 µs (Americas)", "75 µs (Americi)"},
    {StringId::NoSoundOutput, "No sound: {}", "Fără sunet: {}"},
    {StringId::PresetsLabel, "Presets", "Presetări"},
    {StringId::PresetFmBroadcast, "FM radio", "Radio FM"},
    {StringId::PresetAirband, "Airband (AM)", "Bandă aeronautică (AM)"},
    {StringId::PresetAmateur2m, "2 m amateur band", "Banda de 2 m (radioamatori)"},
    {StringId::PresetPmr446, "PMR446 walkie-talkies", "Stații PMR446"},
    {StringId::PresetAdsb, "Aircraft (ADS-B)", "Avioane (ADS-B)"},
    {StringId::PresetMeteor, "Meteor-M weather satellites", "Sateliți meteo Meteor-M"},
    {StringId::FrequencyHelp,
     "Scroll over a digit, or click it and use ↑ ↓, to change that digit. Type digits to enter a "
     "frequency.",
     "Derulează peste o cifră sau dă clic pe ea și folosește ↑ ↓ ca s-o schimbi. Tastează cifre "
     "ca să introduci o frecvență."},
    {StringId::FrequencyFixedHelp, "A recording stays on the frequency it was made on.",
     "O înregistrare rămâne pe frecvența la care a fost făcută."},
    // Device panel
    {StringId::ReceiversTitle, "Receivers", "Receptoare"},
    {StringId::Searching, "Looking for RTL-SDR sticks…", "Se caută stickuri RTL-SDR…"},
    {StringId::NoReceiverFound, "No RTL-SDR stick found. Plug one in, then search again.",
     "Niciun stick RTL-SDR găsit. Conectează unul, apoi caută din nou."},
    {StringId::SearchAgain, "Search again", "Caută din nou"},
    {StringId::Open, "Open", "Deschide"},
    {StringId::SerialNumber, "Serial {}", "Serie {}"},
    {StringId::CloseReceiver, "Close receiver", "Închide receptorul"},
    {StringId::CloseRecording, "Close recording", "Închide înregistrarea"},
    {StringId::TunerLabel, "Tuner", "Tuner"},
    {StringId::IdentityConsistent, "The tuner matches the model the stick reports.",
     "Tunerul corespunde modelului raportat de stick."},
    {StringId::IdentityMismatch,
     "The stick reports itself as an {}, but its tuner isn't the one that model uses. It may be "
     "a counterfeit.",
     "Stickul se declară {}, dar tunerul nu este cel folosit de acest model. Poate fi o "
     "contrafacere."},
    {StringId::IdentityUnverifiable, "A generic stick: it reports no model to check.",
     "Stick generic: nu raportează un model de verificat."},
    {StringId::SampleRateLabel, "Sample rate", "Rată de eșantionare"},
    {StringId::PpmLabel, "Frequency correction (ppm)", "Corecție de frecvență (ppm)"},
    {StringId::BiasTeeLabel, "Bias-tee (antenna power)", "Bias-tee (alimentarea antenei)"},
    {StringId::BiasTeeWarning,
     "The bias-tee sends 4.5 V up the antenna cable to power an LNA or an active antenna. With a "
     "plain antenna, or one that shorts DC, it can damage the stick or the antenna.",
     "Bias-tee trimite 4,5 V pe cablul antenei, ca să alimenteze un LNA sau o antenă activă. Cu o "
     "antenă simplă sau una care scurtcircuitează curentul continuu poate strica stickul sau "
     "antena."},
    {StringId::BiasTeeTurnOn, "Turn on bias-tee", "Pornește bias-tee"},
    {StringId::Cancel, "Cancel", "Renunță"},
    {StringId::RecordingFromFile, "Recording · {} · {}", "Înregistrare · {} · {}"},
    // Banners
    {StringId::RecordingBanner, "Recording {} — {} · {}", "Înregistrare {} — {} · {}"},
    {StringId::RecordingEndedRetune, "Recording ended because the frequency changed. Saved {}.",
     "Înregistrarea s-a oprit pentru că s-a schimbat frecvența. Salvată: {}."},
    {StringId::RecordingFailed, "Recording failed: {}", "Înregistrarea a eșuat: {}"},
    {StringId::ReceiverFailed, "Couldn't open the receiver: {}",
     "Receptorul nu a putut fi deschis: {}"},
    {StringId::Dismiss, "Dismiss", "Închide"},
    // Spectrum
    {StringId::TabSpectrum, "Spectrum", "Spectru"},
    {StringId::FftInfo, "FFT {} · {}/bin", "FFT {} · {}/bin"},
    {StringId::FftSizeLabel, "FFT size", "Mărime FFT"},
    {StringId::SmoothingLabel, "Spectrum smoothing", "Netezirea spectrului"},
    {StringId::WaterfallFloorLabel, "Waterfall floor", "Pragul de jos al waterfall-ului"},
    {StringId::WaterfallCeilingLabel, "Waterfall ceiling", "Pragul de sus al waterfall-ului"},
    {StringId::FitToSignal, "Fit to signal", "Potrivește după semnal"},
    {StringId::TuneHint, "Click to listen here, scroll to step",
     "Clic ca să asculți aici, derulează ca să pășești"},
    // Status bar
    {StringId::StatusVersion, "Zarescope", "Zarescope"},
    {StringId::StatusUtc, "UTC", "UTC"},
    {StringId::StatusLocal, "Local", "Local"},
    {StringId::StatusNoReceiver, "No receiver", "Niciun receptor"},
    {StringId::StatusReplaying, "Replaying", "Redare"},
    {StringId::StatusDropped, "Dropped {} blocks", "{} blocuri pierdute"},
}};

constexpr bool rowsMatchIds() {
    for (size_t i = 0; i < kStrings.size(); ++i) {
        if (static_cast<size_t>(kStrings[i].id) != i || kStrings[i].english.empty() ||
            kStrings[i].romanian.empty()) {
            return false;
        }
    }
    return true;
}
static_assert(rowsMatchIds(), "kStrings needs one row per StringId, in enum order, both languages");

}  // namespace

std::string_view text(StringId id, Language language) {
    const Entry& entry = kStrings[static_cast<size_t>(id)];
    return language == Language::Romanian ? entry.romanian : entry.english;
}

}  // namespace zarescope::ui
